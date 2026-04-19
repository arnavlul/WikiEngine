#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits>

using namespace std;

// --- CONFIGURATION ---
const string INDEX_FILE = "BinsAndTxtx\\index.bin";
const string OFFSET_FILE = "BinsAndTxtx\\offset.txt";

// Adjust this to your approximate total docs to catch garbage integers
const long long MAX_VALID_DOC_ID = 81491764; 
const long long MAX_DOC_FREQ = 7084107;

struct Posting {
    int doc_id;
    float tf_score;
};

int main() {
    // Fast I/O
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    cout << "--- Starting Index Validation ---" << endl;

    ifstream offset_file(OFFSET_FILE);
    if (!offset_file.is_open()) {
        cerr << "Error: Could not open offsets file." << endl;
        return 1;
    }

    ifstream bin_file(INDEX_FILE, ios::binary);
    if (!bin_file.is_open()) {
        cerr << "Error: Could not open binary index file." << endl;
        return 1;
    }

    // 1. Verify Header
    int total_docs;
    bin_file.read(reinterpret_cast<char*>(&total_docs), sizeof(total_docs));
    
    cout << "Header Check: Total Docs = " << total_docs << endl;
    if (total_docs <= 0 || total_docs > MAX_DOC_FREQ) {
        cerr << "CRITICAL FAIL: Header contains garbage value!" << endl;
        return 1;
    }

    string term;
    long long offset;
    long long terms_checked = 0;
    long long errors_found = 0;

    cout << "Scanning terms..." << endl;

    while (offset_file >> term >> offset) {
        terms_checked++;
        
        // A. Seek to Data
        bin_file.seekg(offset);
        if (bin_file.fail()) {
             cerr << "[FAIL] Seek error for term: " << term << " at offset " << offset << endl;
             errors_found++;
             bin_file.clear(); // Reset error state
             continue;
        }

        // B. Read Document Frequency
        int doc_freq;
        bin_file.read(reinterpret_cast<char*>(&doc_freq), sizeof(doc_freq));

        if (bin_file.fail() || doc_freq < 0 || doc_freq > MAX_DOC_FREQ) {
            cerr << "[FAIL] Corrupt DocFreq for term: '" << term << "' (Offset: " << offset << ") -> Value: " << doc_freq << endl;
            errors_found++;
            // We can't verify postings if count is garbage, so skip
            continue; 
        }

        // C. Read Postings (Sanity Check IDs)
        // We don't need to store them, just read and verify
        // To save RAM, we read one by one instead of allocating a huge vector
        for (int i = 0; i < doc_freq; i++) {
            Posting p;
            bin_file.read(reinterpret_cast<char*>(&p), sizeof(p));
            
            if (p.doc_id < 0 || p.doc_id > MAX_VALID_DOC_ID) {
                // Only print the first error per term to avoid spam
                cerr << "[FAIL] Garbage DocID for term: '" << term << "' -> ID: " << p.doc_id << endl;
                errors_found++;
                break; 
            }
        }

        if (terms_checked % 100000 == 0) {
            cout << "Checked " << terms_checked << " terms... (" << errors_found << " errors)" << "\r" << flush;
        }
    }

    offset_file.close();
    bin_file.close();

    cout << "\n\n--- Validation Complete ---" << endl;
    cout << "Total Terms: " << terms_checked << endl;
    cout << "Total Errors: " << errors_found << endl;

    if (errors_found == 0) {
        cout << "✅ SUCCESS: Index Integrity Verified." << endl;
    } else {
        cout << "❌ FAILURE: Index contains corruption." << endl;
    }

    return 0;
}