#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
using namespace std;

struct Posting{
    int id;
    float score;
};

int main(int argc, char* argv[]){

    if(argc < 2){
        cerr << "Incorrect Number of Arguments" << endl;
        return 1;
    }

    int shard_id = stoi(argv[1]);

    string FILE_NAME = "temp_" + to_string(shard_id) + ".txt";
    string OUTPUT_BIN = "chunk_" + to_string(shard_id) + ".bin";
    string OUTPUT_OFFSET = "chunk_offsets_" + to_string(shard_id) + ".txt";

    cout << "----- Processing Shard ID" << shard_id << " -----" << endl;

    unordered_map<string, vector<Posting>> index;

    ifstream inFile;
    static char buffer[1024*1024];
    inFile.rdbuf()->pubsetbuf(buffer, sizeof(buffer));
    inFile.open(FILE_NAME);

    if(!inFile.is_open()){
        cerr << "Could not open input file" << endl;
        return 1;
    }

    string term;
    int doc_id;
    float score;

    int termcounter=0;
    auto abs_start_time = chrono::high_resolution_clock::now();
    auto chunk_start_time = abs_start_time;
    
    while(inFile >> term >> doc_id >> score){
        index[term].push_back({doc_id, score});

        termcounter++;
        if(termcounter % 100000 == 0){
            auto chunk_current_time = chrono::high_resolution_clock::now();
            auto chunk_duration = chrono::duration_cast<chrono::milliseconds>(chunk_current_time - chunk_start_time);
            auto abs_duration = chrono::duration_cast<chrono::seconds>(chunk_current_time - abs_start_time).count();
            int minutes = abs_duration / 60;
            int seconds = abs_duration % 60;

            cout << termcounter << " terms processed. Chunk Time: " << chunk_duration.count() / 1000.0 << " seconds. Total Time: " << minutes << " minutes, " << seconds << " sec.\r";
            chunk_start_time = chunk_current_time; 
        }
    }

    inFile.close();

    cout << "\n----- Done building Map -----" <<endl;

    ofstream bin_file(OUTPUT_BIN, ios::binary);
    ofstream offset_file(OUTPUT_OFFSET);

    if(!bin_file.is_open() || !offset_file.is_open()){
        cerr << "Bin file or Offset File not opening" << endl;
        return 1;
    }

    int totalpairs = index.size();
    int paircount = 0;
    abs_start_time = chrono::high_resolution_clock::now();
    chunk_start_time = abs_start_time;

    for(const auto& pair : index){
        const string& term = pair.first;
        const vector<Posting>& posts = pair.second;

        long long pos = bin_file.tellp();
        offset_file << term << " " << pos << "\n";

        int docfreq = posts.size();
        bin_file.write(reinterpret_cast<const char*>(&docfreq), sizeof(docfreq));

        bin_file.write(reinterpret_cast<const char*>(posts.data()), docfreq * sizeof(Posting));

        paircount++;
        if(paircount % 100000 == 0){
            auto chunk_current_time = chrono::high_resolution_clock::now();
            auto chunk_duration = chrono::duration_cast<chrono::milliseconds>(chunk_current_time - chunk_start_time);
            auto abs_duration = chrono::duration_cast<chrono::seconds>(chunk_current_time - abs_start_time).count();
            int minutes = abs_duration / 60;
            int seconds = abs_duration % 60;

            cout << paircount << " terms processed. Progress: " << (paircount*100.0)/totalpairs << "%. Chunk Time: " << chunk_duration.count() / 1000.0 << " seconds. Total Time: " << minutes << " minutes, " << seconds << " sec.\r";
            chunk_start_time = chunk_current_time; 
        }
    }

    bin_file.close();
    offset_file.close();

    cout << "\n----- Saved " << OUTPUT_BIN << " -----" << endl;

    return 0;
}