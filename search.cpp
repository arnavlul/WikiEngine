#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <unordered_set>
#include "json.hpp"
#include "check_stem.hpp"

using json = nlohmann::json;

const string INDEX_FILE = "BinsAndTxtx\\index.bin";
const string OFFSET_FILE = "BinsAndTxtx\\offset.txt";
const string DOC_INFO_FILE=  "data_files\\doc_info.jsonl";
const string PYTHON_STEMMER_SCRIPT = "stemmer_bridge.py";
const string STOPWORD_FILE = "stopwords.txt";
const string PAGERANK_SCORES_PATH = "pagerank_scores.csv";
const float alpha = 0.2;

unordered_map<int, pair<string, int>> doc_details;
double avg_doc_length = 0.0;

unordered_set<string> stopwords;
unordered_map<int, double> pagerank_scores;

struct Posting{
    int doc_id;
    float tf_score;
};

struct SearchResult{
    int doc_id;
    double score;    
    string title;
};


void load_titles(){
    cout << "Loading documents into hashmap..." << endl;
    ifstream infile(DOC_INFO_FILE);
    if(!infile.is_open()){
        cerr << "Error: Could not open doc file";
        return;
    }

    doc_details.reserve(7084107);

    string line;
    long long count = 0;

    auto start_time = chrono::high_resolution_clock::now();
    auto batch_start_time = start_time; 
    long long total_corpus_length = 0;

    while(getline(infile, line)){
        try{
            auto j = json::parse(line);
            int doc_id = j["id"];
            string doc_title = j["title"];
            int doc_length = j["len"];

            doc_details[doc_id] = {doc_title, doc_length};
            total_corpus_length += doc_length;

            count++;
            if(count % 100000 == 0){
                auto batch_end_time = chrono::high_resolution_clock::now();
                auto batch_duration = chrono::duration_cast<chrono::milliseconds>(batch_end_time - batch_start_time);
                auto total_duration = chrono::duration_cast<chrono::seconds>(batch_end_time - start_time).count();
                int minutes = total_duration / 60;
                int seconds=  total_duration % 60;
                cout << count << " documents processed. Batch Time: " << batch_duration.count() / 1000.0 << " sec. Total Time: " << minutes << " min, " << seconds << " sec.\r";
                batch_start_time = batch_end_time;
            }
        }
        catch(exception &e){
            cerr << "Error: " << e.what() << endl;
            return;
        }
    }
    cout << endl;
    infile.close();
    if(doc_details.size() > 0){
        avg_doc_length = (double)total_corpus_length / count; 
    }
    cout << "Titles loaded. Total unique docs: " << doc_details.size() << endl;
    return;
}

string get_title(int doc_id){
    auto it = doc_details.find(doc_id);
    if(it != doc_details.end()) return it->second.first;
    return "Unknown Title";
}

void load_stopwords(){
    ifstream stopfile(STOPWORD_FILE);
    cout << "Loading Stopwords...\n";
    if(!stopfile.is_open()){
        cerr << "Stopwords could not load" << endl;
        return;
    }

    string word;
    while(stopfile >> word){
        stopwords.insert(word);
    }
    
    cout << "Stopwords Loaded\n";
}

void load_pagerank_scores(){
    cout << "Loading pagerank scores..." << endl;
    ifstream infile(PAGERANK_SCORES_PATH);
    if(!infile.is_open()){
        cerr << "Error: Pagerank file could not be opened";
        exit(1);
    }


    pagerank_scores.reserve(7084107);
    string line;
    long long count = 0;

    while(getline(infile, line)){
        size_t comma_pos = line.find(',');
        if(comma_pos == string::npos) continue;
        try{
            int doc_id = stoi(line.substr(0, comma_pos));
            double score = stod(line.substr(comma_pos+1));

            pagerank_scores[doc_id] = score;
            count++;

            if(count % 10000 == 0){
                cout << count << " pages processed for pagerank.\r" << flush;
            }

        }
        catch(exception &e){
            cout << "Error: " << e.what() << endl;
            continue;
        }
    }

    cout << "\nPagerank scores loaded..." << endl;
    return;

}


int main(){

    cout << "----- Wikipedia Search Engine -----" << endl;
    cout << "Loading Dictionary..." << endl;

    load_titles();
    load_stopwords();   
    load_pagerank_scores(); 

    unordered_map<string, long long> offsets;
    ifstream offset_file(OFFSET_FILE);

    if(!offset_file.is_open()){
        cerr << "Erorr: Output file could not open";
        return 1;
    }

    string term;
    long long pos;

    while(offset_file >> term >> pos){
        offsets[term] = pos;
    }

    offset_file.close();

    ifstream bin_file(INDEX_FILE, ios::binary);
    
    if(!bin_file.is_open()){
        cerr << "Error: Could not open bin file";
        return 1;
    }

    int total_docs;
    bin_file.read(reinterpret_cast<char*>(&total_docs), sizeof(total_docs));
    cout << "Total Documents: " << total_docs << endl;
    
    PythonStemmer pstemmer(PYTHON_STEMMER_SCRIPT);

    string input_line;
    cout << "\nEnter query or 'exit";

    bin_file.seekg(0, ios::end);
    const std::streamoff file_size = bin_file.tellg();
    bin_file.seekg(0, ios::beg);

    while(true){
        cout << "\nSearch > ";
        if(!getline(cin, input_line) || input_line == "exit") break;

        if(input_line.empty()) continue;

        auto start_time = chrono::high_resolution_clock::now();

        stringstream ss(input_line);
        string raw_term;
        vector<string> search_term;

        while(ss >> raw_term){
            transform(raw_term.begin(), raw_term.end(), raw_term.begin(), ::tolower);
            
            if(stopwords.find(raw_term) != stopwords.end()) continue;

            string stemmed = pstemmer.stem(raw_term);
            search_term.push_back(stemmed);
        }

        unordered_map<int, double> doc_scores;

        for(const string& term : search_term){
            if(offsets.find(term) == offsets.end()){
                cout << "Term not found in index" << endl;
                continue;
            } 

            long long byte_offset = offsets[term];
            cout << "Byte offset: " << byte_offset;
            if(bin_file.fail() || bin_file.bad()) {
                cerr << "  WARNING: Stream in bad state before seek!" << endl;
                bin_file.clear();
            }

            bin_file.seekg(byte_offset);

            if(bin_file.fail()) {
                cerr << "  ERROR: Seek failed!" << endl;
                bin_file.clear();
                continue;
            }

            cout << "  Current position: " << bin_file.tellg() << endl;

            int doc_freq;
            bin_file.read(reinterpret_cast<char*>(&doc_freq), sizeof(doc_freq));

            if(bin_file.fail()) {
                cerr << "  ERROR: Failed to read doc_freq!" << endl;
                bin_file.clear();
                continue;
            }

            cout << "  doc_freq: " << doc_freq << endl;

            if(doc_freq <= 0 || doc_freq > 7084107) {
                cerr << "  ERROR: Invalid doc_freq value!" << endl;
                continue;
            }

            if(doc_freq == 0) continue;

            double idf = log((total_docs - doc_freq + 0.5) / (doc_freq + 0.5)); 
            idf = max(0.0, idf);

            vector<Posting> postings(doc_freq);

            bin_file.read(reinterpret_cast<char*>(postings.data()), doc_freq * sizeof(Posting));

            const double k1 = 1.2;
            const double b = 0.75;

            for(const auto& p : postings){
                int doc_len = int(avg_doc_length);
                auto it = doc_details.find(p.doc_id);
                if(it != doc_details.end()){
                    doc_len = it->second.second;
                }

                float raw_freq = p.tf_score * doc_len;

                double numerator = raw_freq * (k1+1);
                double denominator = raw_freq + k1 * (1 - b + b * ((double) doc_len / avg_doc_length));
                double bm25_score = idf * (numerator / denominator);

                double pgscore = pagerank_scores[p.doc_id];
                double pgnorm = log(1.0 + pgscore * pagerank_scores.size());
                doc_scores[p.doc_id] += bm25_score + alpha * pgnorm;
            }
        }
        
        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
        
        if(doc_scores.empty()){
            cout << "No results found. Time: " << duration.count() << " ms.";
            continue;
        }

        vector<SearchResult> ranked_results;
        ranked_results.reserve(doc_scores.size());
        for(auto const& [id, score] : doc_scores){
            ranked_results.push_back({id, score, get_title(id)});
        }

        sort(ranked_results.begin(), ranked_results.end(), [](const SearchResult &a, const SearchResult &b){
            return a.score > b.score;
        });

        cout << "Found " << ranked_results.size() << " raw matches. Filtering results..." << endl;
        cout << "Time: " << duration.count() << " ms." << endl;
        
        int printed_count = 0;
        int max_results = 10;
        
        // Loop through potentially more than 10 results to find 10 valid ones
        // We check up to 50 (or all) to find enough good links
        for (int i = 0; i < ranked_results.size() && printed_count < max_results; i++) {
            
            // 1. Check for "disambiguation" in title (Case-insensitive check is safer)
            string title_lower = ranked_results[i].title;
            transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
            
            if (title_lower.find("disambiguation") != string::npos) {
                continue; // SKIP this result
            }

            // 2. Print Valid Result
            string link_title = ranked_results[i].title;
            replace(link_title.begin(), link_title.end(), ' ', '_');

            // Handle missing titles gracefully
            if (link_title.empty() || link_title == "Unknown Title") {
                 cout << "  [" << printed_count + 1 << "] (ID: " << ranked_results[i].doc_id << ") - Unknown Title"
                     << "\n      Score: " << fixed << setprecision(4) << ranked_results[i].score
                     << "\n      Link: https://en.wikipedia.org/wiki/Special:Redirect/page/" << ranked_results[i].doc_id << endl;
            } else {
                cout << "  [" << printed_count + 1 << "] " << ranked_results[i].title 
                     << "\n      Score: " << fixed << setprecision(4) << ranked_results[i].score
                     << " | ID: " << ranked_results[i].doc_id
                     << "\n      Link: https://en.wikipedia.org/wiki/" << link_title 
                     << "\n      Link: https://en.wikipedia.org/wiki/Special:Redirect/page/" << ranked_results[i].doc_id << endl;
            }
            
            printed_count++;
        }
        
        if (printed_count == 0) {
            cout << "  (No relevant results found after filtering)" << endl;
        }
    }

    return 0;
}