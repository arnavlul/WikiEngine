#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isAutocompleteReady(false) // Initialize to false
    , isSearchReady(false)       // Initialize to false
{
    ui->setupUi(this);

    // 1. UI Styling & Initial State
    ui->resultsList->setVisible(false);

    // Force High Contrast Colors for Visibility
    ui->resultsList->setStyleSheet(
        "QListWidget { "
        "   border: 1px solid lightgray; "
        "   background: #ffffff; "
        "   color: #000000; "
        "   font-size: 14px; "
        "}"
        "QListWidget::item:hover { background: #e0e0e0; color: #000000; }"
        "QListWidget::item:selected { background: #d0d0d0; color: #000000; }"
        );

    // DISABLE EVERYTHING INITIALLY
    ui->searchBox->setEnabled(false);
    ui->searchBox->setPlaceholderText("Initializing Engines... Please Wait...");
    ui->searchBtn->setEnabled(false); // Also disable the button

    // --- PATHS ---
    QString workingDir = "C:/Users/Arnav/Desktop/Programming/WikiEngine";

    // Updated paths for both engines
    QString autoProg = "C:/Users/Arnav/Desktop/Programming/WikiEngine/forGUI/autoCompleteTrie.exe";
    QString searchProg = "C:/Users/Arnav/Desktop/Programming/WikiEngine/forGUI/search.exe";

    // 2. Start Autocomplete
    autocompleteProcess = new QProcess(this);
    // Set working directory to the main folder so it finds data_files/trie.bin
    autocompleteProcess->setWorkingDirectory(workingDir);

    // Merge stderr into stdout to catch all output
    autocompleteProcess->setProcessChannelMode(QProcess::MergedChannels);

    // Connect signals BEFORE starting
    connect(autocompleteProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleAutocompleteOutput);

    if (QFile::exists(autoProg)) {
        autocompleteProcess->start(autoProg);
        qDebug() << "Started autoCompleteTrie.exe";
    } else {
        qDebug() << "Missing:" << autoProg;
        ui->searchBox->setPlaceholderText("Error: Autocomplete exe missing");
    }

    // 3. Start Search Engine
    searchProcess = new QProcess(this);
    searchProcess->setWorkingDirectory(workingDir);

    // Add virtual environment to PATH so Python scripts can find NLTK
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString venvPath = "C:/Users/Arnav/Desktop/Programming/WikiEngine/venv/Scripts";
    QString currentPath = env.value("PATH");
    env.insert("PATH", venvPath + ";" + currentPath);
    env.insert("VIRTUAL_ENV", "C:/Users/Arnav/Desktop/Programming/WikiEngine/venv");
    searchProcess->setProcessEnvironment(env);

    // Merge stderr into stdout to catch all output
    searchProcess->setProcessChannelMode(QProcess::MergedChannels);

    // Connect signals BEFORE starting
    connect(searchProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleSearchOutput);
    connect(searchProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error){
        qDebug() << "Search Process Error:" << error;
    });
    connect(searchProcess, &QProcess::stateChanged, this, [this](QProcess::ProcessState state){
        qDebug() << "Search Process State:" << state;
    });

    if (QFile::exists(searchProg)) {
        searchProcess->start(searchProg);
        qDebug() << "Started search.exe";
    } else {
        qDebug() << "Missing:" << searchProg;
        ui->searchBox->setPlaceholderText("Error: Search exe missing");
    }

    // Event Handlers
    connect(ui->resultsList, &QListWidget::itemClicked, this, &MainWindow::on_resultsList_itemClicked);
    connect(ui->searchBox, &QLineEdit::returnPressed, this, &MainWindow::on_searchBtn_clicked);
}

MainWindow::~MainWindow()
{
    if (autocompleteProcess->isOpen()) autocompleteProcess->terminate();
    if (searchProcess->isOpen()) searchProcess->terminate();
    delete ui;
}

// --- READINESS CHECKER ---
void MainWindow::checkReadyState() {
    // Only enable if BOTH are ready
    if (isAutocompleteReady && isSearchReady) {
        ui->searchBox->setEnabled(true);
        ui->searchBtn->setEnabled(true);
        ui->searchBox->setPlaceholderText("Search Wikipedia...");
        ui->searchBox->setFocus();
    } else if (isAutocompleteReady) {
        ui->searchBox->setPlaceholderText("Waiting for Search Engine...");
    } else if (isSearchReady) {
        ui->searchBox->setPlaceholderText("Waiting for Autocomplete...");
    }
}

// --- AUTOCOMPLETE ---
void MainWindow::on_searchBox_textChanged(const QString &arg1)
{
    if (arg1.isEmpty()) {
        ui->resultsList->clear();
        ui->resultsList->setVisible(false);
        return;
    }
    autocompleteProcess->write((arg1 + "\n").toUtf8());
}

void MainWindow::handleAutocompleteOutput()
{
    QByteArray data = autocompleteProcess->readAllStandardOutput();
    QString output = QString::fromUtf8(data);

    // SIGNAL CHECK - only process if not yet ready
    if (!isAutocompleteReady && output.contains("Index ready")) {
        isAutocompleteReady = true;
        checkReadyState();
        return;
    }

    ui->resultsList->clear();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    bool hasResults = false;

    for (const QString &line : std::as_const(lines)) {
        QString clean = line.trimmed();
        if (clean.isEmpty() || clean.startsWith(">") || clean.contains("Index ready")) continue;

        QStringList parts = clean.split('|');
        if (parts.size() >= 2) {
            QString title = parts[0];
            QString link = parts[1];
            if (title.isEmpty()) continue;

            QListWidgetItem *item = new QListWidgetItem("🔍 " + title);
            item->setData(Qt::UserRole, link);
            ui->resultsList->addItem(item);
            hasResults = true;
        }
    }

    if (hasResults) {
        ui->resultsList->setVisible(true);
        int h = ui->resultsList->sizeHintForRow(0) * ui->resultsList->count() + 5;
        ui->resultsList->setFixedHeight(std::min(h, 300));
    } else {
        ui->resultsList->setVisible(false);
    }
}

// --- SEARCH ---
void MainWindow::on_searchBtn_clicked()
{
    QString query = ui->searchBox->text();
    if (query.isEmpty()) return;

    // Clear list and show loading
    ui->resultsList->clear();
    QListWidgetItem *loading = new QListWidgetItem("Searching for \"" + query + "\"...");
    loading->setTextAlignment(Qt::AlignCenter);
    ui->resultsList->addItem(loading);
    ui->resultsList->setVisible(true);
    ui->resultsList->setFixedHeight(50);

    qDebug() << "Sending search query:" << query;
    searchProcess->write((query + "\n").toUtf8());
}

void MainWindow::handleSearchOutput()
{
    QByteArray data = searchProcess->readAllStandardOutput();
    QString output = QString::fromUtf8(data);

    qDebug() << "=== RAW Search Output ===";
    qDebug() << output;
    qDebug() << "=== END Raw Output ===";

    // SIGNAL CHECK - only process if not yet ready
    if (!isSearchReady && output.contains("Search ready")) {
        qDebug() << "Search engine is now ready";
        isSearchReady = true;
        checkReadyState();
        return;
    }

    // Skip initialization messages only if search is not ready yet
    if (!isSearchReady) {
        if (output.contains("Total Documents") || output.contains("Dictionary loaded")) {
            qDebug() << "Skipping initialization message";
            return;
        }
    }

    // If output is empty or only whitespace, don't process
    if (output.trimmed().isEmpty()) {
        qDebug() << "Empty output received";
        return;
    }

    ui->resultsList->clear();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    bool hasResults = false;

    qDebug() << "Processing" << lines.count() << "lines";

    for (const QString &line : std::as_const(lines)) {
        QString clean = line.trimmed();

        qDebug() << "Processing line:" << clean;

        // Skip empty lines, prompts, and status messages
        if (clean.isEmpty() ||
            clean.startsWith("Search >") ||
            clean.startsWith("Enter query") ||
            clean.startsWith("Found") ||
            clean.contains("Search ready") ||
            clean.contains("Total Documents") ||
            clean.contains("Dictionary loaded")) {
            qDebug() << "  -> Skipped (status/prompt)";
            continue;
        }

        // Format: Title|Score|DocID|URL
        QStringList parts = clean.split('|');
        qDebug() << "  -> Split into" << parts.size() << "parts";

        if (parts.size() >= 4) {
            QString title = parts[0];
            QString score = parts[1];
            QString link = parts[3];

            qDebug() << "  -> Valid result:" << title;

            QString displayText = QString("%1\n      Relevance: %2").arg(title, score);

            QListWidgetItem *item = new QListWidgetItem("📄 " + displayText);
            item->setData(Qt::UserRole, link);
            item->setFont(QFont("Segoe UI", 10));

            ui->resultsList->addItem(item);
            hasResults = true;
        } else {
            qDebug() << "  -> Not enough parts for a valid result";
        }
    }

    qDebug() << "Total results found:" << (hasResults ? "YES" : "NO");

    if (hasResults) {
        ui->resultsList->setVisible(true);
        ui->resultsList->setFixedHeight(400);
    } else {
        // Only show "no results" if we're sure there were no valid results
        // and this wasn't just a status message
        bool hasContent = false;
        for (const QString &line : std::as_const(lines)) {
            if (!line.trimmed().isEmpty() &&
                !line.contains("Search ready") &&
                !line.contains("Total Documents") &&
                !line.contains("Dictionary loaded")) {
                hasContent = true;
                break;
            }
        }

        if (hasContent) {
            ui->resultsList->addItem("No results found.");
            ui->resultsList->setVisible(true);
            ui->resultsList->setFixedHeight(50);
        }
    }
}

void MainWindow::on_resultsList_itemClicked(QListWidgetItem *item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty() && url.startsWith("http")) {
        QDesktopServices::openUrl(QUrl(url));
    } else {
        QString title = item->text().replace("🔍 ", "");
        ui->searchBox->setText(title);
        on_searchBtn_clicked();
    }
}
