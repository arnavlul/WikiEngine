#include <iostream>
#include <map>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <string>
#include <limits>
#include "json.hpp"
using json = nlohmann::json;
using namespace std;

const string DOC_INFO_PATH = "data_files\\doc_info.jsonl";
const string TRIE_BINARY_PATH = "trie.bin";

struct TrieNode{
    map<char, TrieNode*> children;
    bool isEnd;
    string fullTitle;
    int score;
    int pageId;

    TrieNode(){
        isEnd = false;
        fullTitle = "";
        score = 0;
        pageId = 0;
    }

};

struct Suggestion{
    string title;
    int pageId;
    int score;
};

class Trie{
private:
    TrieNode* root;

    void dfs(TrieNode* node, vector<Suggestion>& results){

        if(node->isEnd){
                results.push_back({node->fullTitle, node->score});
        }

        for(auto const&[key, child] : node->children){
            dfs(child, results);
        }
    }

    void deleteNodes(TrieNode* node){
        if(!node) return;
        for(auto const& [key, child] : node->children){
            deleteNodes(child);
        }
        delete node;
    }

    void serialize(TrieNode* node, ofstream& out){
        if(!node) return;

        out.write(reinterpret_cast<const char*>(&node->isEnd), sizeof(node->isEnd));
        
        if(node->isEnd){
            out.write(reinterpret_cast<const char*>(&node->score), sizeof(node->score));
            out.write(reinterpret_cast<const char*>(&node->pageId), sizeof(node->pageId));

            size_t len = node->fullTitle.length();
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            out.write(node->fullTitle.c_str(), len);              
        }

        size_t num_children = node->children.size();
        out.write(reinterpret_cast<const char*>(&num_children), sizeof(num_children));

        for(auto const &[key, child] : node->children){
            out.write(reinterpret_cast<const char*>(&key), sizeof(key));
            serialize(child, out);
        }
    }

    TrieNode* deserialize(ifstream& in){
        TrieNode* node = new TrieNode();

        in.read(reinterpret_cast<char*>(&node->isEnd), sizeof(node->isEnd));
        if(in.eof()){
            delete node;
            return nullptr;
        }

        if(node->isEnd){
            in.read(reinterpret_cast<char*>(&node->score), sizeof(node->score));
            in.read(reinterpret_cast<char*>(&node->pageId), sizeof(node->pageId));

            size_t len;
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            node->fullTitle.resize(len);
            in.read(node->fullTitle.data(), len);
        }

        size_t num_children;
        in.read(reinterpret_cast<char*>(&num_children), sizeof(num_children));

        for(size_t i=0; i<num_children; i++){
            char key;
            in.read(reinterpret_cast<char*>(&key), sizeof(key));
            node->children[key] = deserialize(in);
        }

        return node;
    }


public:

    Trie(){
        root = new TrieNode();
    }

    ~Trie(){
        deleteNodes(root);
    }

    void insert(const string& title, int score, int pageId){
        TrieNode* current = root;
        for(char ch : title){
            if(current->children.find(ch) == current->children.end()){
                current->children[ch] = new TrieNode();
            }
            current = current->children[ch];
        }
        current->isEnd = true;
        current->fullTitle = title;
        current->score = score; 
        current->pageId = pageId;
    }

    vector<Suggestion> getSuggestion(string prefix, int limit = 10){
        TrieNode* current = root;
        vector<Suggestion> candidates;

        for(char ch : prefix){
            if(current->children.find(ch) == current->children.end()){
                return {};
            }
            current = current->children[ch];
        }

        dfs(current,candidates);

        sort(candidates.begin(), candidates.end(), [](const Suggestion&a, const Suggestion& b){
            return a.score > b.score;
        });
        
        candidates.resize(limit);
        return candidates;
    }

    void saveToDisk(const string& filename){
        ofstream outfile(filename, ios::binary);
        serialize(root, outfile);
        outfile.close();
    }

    bool loadFromDisk(const string& filename){
        ifstream infile(filename, ios::binary);
        if(!infile.is_open()) return false;
        deleteNodes(root);

        root = deserialize(infile);

        return root != nullptr && root->children.size() > 0;    
    }

    bool buildFromJSON(const string& filename){
        cout << "\n----- Building Trie from JSONL ------" << endl;
        ifstream infile(filename);
        static char buffer[1024*1024];
        infile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

        if(!infile.is_open()){
            cerr << "Error: JSONL File not opening";
            return false;
        }

        string line;
        long long count = 0;

        auto start_time = chrono::high_resolution_clock::now();
        auto batch_start_time = start_time;
        cout << "Reading and Writing From JSONL File: " << endl;
        while(getline(infile, line)){
            try{
                auto j = json::parse(line);
                int pageId = j["id"];
                int len = j["len"];
                string title = j["title"];

                this->insert(title, len, pageId);
                count++;

                if(count%10000 == 0){
                    auto current_time = chrono::high_resolution_clock::now();
                    auto batch_duration = chrono::duration_cast<chrono::milliseconds>(current_time-batch_start_time);
                    auto total_duration = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
                    int minutes = total_duration / 60;
                    int seconds = total_duration % 60;
                    cout << "Loaded " << count << " docs. Batch Time: " << batch_duration.count()/1000.0 << " sec. Total Time: " << minutes << " min, " << seconds << " sec.\r";
                    batch_start_time = current_time;
                }
            }
            catch (const json::parse_error& e) {
                cerr << "JSON Parse Error on line " << count + 1 << ": " << e.what() << endl;
                continue;
            }
            catch (const json::type_error& e) {
                // This catches errors like trying to read j["id"] as an int when it's a string.
                cerr << "JSON Type Error on line " << count + 1 << ": " << e.what() << endl;
                continue;
            }
            catch(exception& e){
                cerr << "Error: " << e.what() << endl;
                continue;
            }
        }

        infile.close();
        auto current_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
        int minutes = duration / 60;
        int seconds = duration % 60;

        cout << "----- Finished Building Trie from JSONL -----" << endl;
        cout << "Time Taken: " << minutes << " min, " << seconds << " sec." << endl;
        return true;
    }

};


int main(int argc, char* argv[]){

    Trie trie;
    string mode = (argc > 1) ? argv[1] : "run";

    cout << "----- AutoComplete Trie -----" << endl;

    if(mode == "build"){
        if(trie.buildFromJSON(DOC_INFO_PATH)){
            trie.saveToDisk(TRIE_BINARY_PATH);
            cout << "----- Trie saved to Disk -----" << endl;
        }
        return 0;
    }

    bool loaded = trie.loadFromDisk(TRIE_BINARY_PATH);
    
    if(!loaded){
        cerr << "Error: could not load binary file" << endl;
        return 1;
    }

    cout << "Index ready" << endl;

    string input;
    cout << "\nType a prefix to search (or 'exit'): " << endl;


    while(true){
        cout << "> ";
        if(!getline(cin, input) || input == "exit") break;
        if(input.empty()) continue;

        auto start_time = chrono::high_resolution_clock::now();
        vector<Suggestion> sg = trie.getSuggestion(input);
        auto end_time = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);

        if(sg.empty()){
            cout << "No suggestions found in " << duration.count() / 1000.0 << " sec." << endl;
            continue;
        }
        else{
            cout << "Found " << sg.size() << " suggestions in " << duration.count() / 1000.0  << " sec." << endl;
            for(int i=0; i<sg.size(); i++){
                const Suggestion& s = sg[i];

                cout << " [" << i+1 << "] ID: " << s.pageId << " Score: " << s.score << " Title: " << s.title << " -> https://en.wikipedia.org/wiki/Special:Redirect/page/" << s.pageId << endl;
            }
        }

    }

    return 0;
}