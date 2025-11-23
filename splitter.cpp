#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include "json.hpp"
#include <chrono>
#include <functional>
using json = nlohmann::json;
using namespace std;

string TF_FILE_PATH = "data_files\\tf_data.jsonl";

int main(){

    const int NUM_SHARDS = 32; // Ideally power of 2
    const int SHARD_MASK = NUM_SHARDS-1; 
    vector<ofstream> shards(NUM_SHARDS);
    for(int i=0; i<NUM_SHARDS; i++){
        string fileName = "temp_" + to_string(i) + ".txt";
        shards[i].open(fileName);
        if(!shards[i].is_open()){
            cerr << "Error, shard not opening: " << fileName << endl;
            return 1;
        }
    }

    hash<string> hasher;

    ifstream tfFile;
    static char buffer[1024*1024];
    tfFile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    tfFile.open(TF_FILE_PATH);


    if(!tfFile.is_open()){
        cout << "Error opening tf_data.jsonl";
        return 1;
    }

    string line;
    long long termCounter = 0;
    line.reserve(10000);

    auto batch_start_time = chrono::high_resolution_clock::now();
    auto abs_start_time = chrono:: high_resolution_clock::now();

    int doc_id;
    while(getline(tfFile, line)){
        try{
            auto j = json::parse(line);
            doc_id = j["id"];

            for(auto& item : j["terms"].items()){
                string term = item.key();
                float score = item.value();

                if(term.length() < 2){
                    continue;
                }

                size_t h = hasher(term);
                int bucket = h & SHARD_MASK;
                shards[bucket] << term << " " << doc_id << " " << score << '\n';
                termCounter++;
            }
            if(termCounter % 1000 == 0){
                auto current_time = chrono::high_resolution_clock::now();
                auto batch_duration = chrono::duration_cast<chrono::milliseconds>(current_time - batch_start_time);
                auto abs_duration = chrono::duration_cast<chrono::seconds>(current_time - abs_start_time).count();
                int minutes = abs_duration / 60;
                int seconds = abs_duration % 60;

                cout << termCounter << " lines processed. Batch_Time: " << batch_duration.count() / 1000.0 << " seconds. Total time: " << minutes << " min, " << seconds << " sec." << endl;

                batch_start_time = current_time;
            }

        }
        catch(const json::parse_error& e){
            cout << "Json parsing error on docID: " << doc_id << ". Error: " << e.what() << endl;
            continue;
        }
        catch(const exception& e){
            cout << "Standard error: " << e.what() << endl;
            continue;
        }   
    }

    tfFile.close();
    for(int i=0; i<NUM_SHARDS; i++) shards[i].close();

    cout << "----- Processing Complete -----" << endl;
    cout << termCounter << " terms parsed";



    return 0;
}