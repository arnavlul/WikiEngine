#include <iostream>
#include <map>
#include <fstream>
#include <algorithm>
#include <string>
#include <limits>
#include "json.hpp"
using json = nlohmann::json;
using namespace std;

const string DOC_INFO_PATH = "C:\\Users\\Arnav\\Desktop\\Programming\\WikiEngine\\data_files\\doc_info.jsonl";
const string TRIE_BINARY_PATH = "C:\\Users\\Arnav\\Desktop\\Programming\\WikiEngine\\trie2.bin";
const string PAGERANK_SCORES_PATH = "C:\\Users\\Arnav\\Desktop\\Programming\\WikiEngine\\pagerank_scores.csv";

unordered_map<int, double> pagerank_scores;

struct TrieNode{
    map<char, TrieNode*> children;
    bool isEnd;
    string fullTitle;
    double score;
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
    double score;
};

class Trie{
private:
    TrieNode* root;
    long long nodesSaved;
    long long totalNodes;

    void dfs(TrieNode* node, vector<Suggestion>& results){

        if(node->isEnd){
                results.push_back({node->fullTitle, node->pageId, node->score});
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

        nodesSaved++;


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
        nodesSaved = 0;
        totalNodes = 1;
    }


    void insert(const string& title, double score, int pageId){
        TrieNode* current = root;
        for(char ch : title){
            ch = tolower(ch);
            if(current->children.find(ch) == current->children.end()){
                current->children[ch] = new TrieNode();
                totalNodes++;
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
            ch = tolower(ch);
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
        ifstream infile(filename);
        static char buffer[1024*1024];
        infile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));

        if(!infile.is_open()){
            cerr << "Error: JSONL File not opening";
            return false;
        }

        string line;
        long long count = 0;

        while(getline(infile, line)){
            try{
                auto j = json::parse(line);
                int pageId = j["id"];
                double len;
                auto it = pagerank_scores.find(pageId);
                if(it == pagerank_scores.end()){
                    len = 0;
                }
                else{
                    len = it->second;
                }
                string title = j["title"];

                this->insert(title, len, pageId);
                count++;


            }
            catch (const json::parse_error& e) {
                cerr << "Error: JSON Parse Error on line " << count + 1 << ": " << e.what() << endl;
                continue;
            }
            catch (const json::type_error& e) {
                // This catches errors like trying to read j["id"] as an int when it's a string.
                cerr << "Error: JSON Type Error on line " << count + 1 << ": " << e.what() << endl;
                continue;
            }
            catch(exception& e){
                cerr << "Error: " << e.what() << endl;
                continue;
            }
        }

        infile.close();
        return true;
    }

};

void load_pagerank_scores(){
    ifstream infile(PAGERANK_SCORES_PATH);
    if(!infile.is_open()){
        cerr << "Error: Pagerank file could not be opened";
        exit(1);
    }


    pagerank_scores.reserve(7084107);
    string line;

    while(getline(infile, line)){
        size_t comma_pos = line.find(',');
        if(comma_pos == string::npos) continue;
        try{
            int doc_id = stoi(line.substr(0, comma_pos));
            double score = stod(line.substr(comma_pos+1));

            pagerank_scores[doc_id] = score;

        }
        catch(exception &e){
            cout << "Error: " << e.what() << endl;
            continue;
        }
    }

    return;

}


int main(int argc, char* argv[]){

    Trie trie;
    string mode = (argc > 1) ? argv[1] : "run";


    if(mode == "build"){
        load_pagerank_scores();
        if(trie.buildFromJSON(DOC_INFO_PATH)){
            trie.saveToDisk(TRIE_BINARY_PATH);
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


    while(true){
        if(!getline(cin, input) || input == "exit") break;
        if(input.empty()) continue;

        vector<Suggestion> sg = trie.getSuggestion(input);

        if(sg.empty()){
            cout << "No suggestions found" << endl;
            continue;
        }
        else{
            for(int i=0; i<sg.size(); i++){
                const Suggestion& s = sg[i];

                cout << s.title << "|https://en.wikipedia.org/wiki/Special:Redirect/page/" << s.pageId << endl;
            }
        }

    }

    return 0;
}