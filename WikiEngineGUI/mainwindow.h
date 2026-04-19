#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_searchBox_textChanged(const QString &arg1);
    void on_searchBtn_clicked();
    void handleAutocompleteOutput();
    void handleSearchOutput();
    void on_resultsList_itemClicked(QListWidgetItem *item);

private:
    Ui::MainWindow *ui;
    QProcess *autocompleteProcess;
    QProcess *searchProcess;

    // Logic to track if engines are ready
    bool isAutocompleteReady;
    bool isSearchReady;
    void checkReadyState(); // Helper to enable UI only when both are true
};
#endif // MAINWINDOW_H
