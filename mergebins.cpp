#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

const int NUM_SHARDS = 32;
const string FINAL_INDEX = "index.bin";
const string FINAL_OFFSET = "offset.txt";
const int TOTAL_DOCS = 7084107;


int main(){

    ofstream final_bin(FINAL_INDEX, ios::binary);
    ofstream final_offset(FINAL_OFFSET);

    if(!final_bin.is_open() || !final_offset.is_open()){
        cerr << "Final bin or Final Offset not opening";
        return 1;
    }

    final_bin.write(reinterpret_cast<const char*>(&TOTAL_DOCS), sizeof(TOTAL_DOCS));

    for(int i=0; i<NUM_SHARDS; i++){
        string chunk_bin_name = "chunk_" + to_string(i) + ".bin";
        string chunk_offset_name = "chunk_offsets_" + to_string(i) + ".txt";

        cout << "----- Merging File " << i << " -----" <<endl;
        
        long long global_base_offset = final_bin.tellp();
        ifstream chunk_offset(chunk_offset_name);
        string term;
        long long localpos;

        if(!chunk_offset.is_open()){
            cerr << "Chunk offset " << i << " did not open";
            return 1;
        }

        while(chunk_offset >> term >> localpos){
            long long finalpos = global_base_offset + localpos;

            final_offset << term << " " << finalpos << '\n';
        }

        chunk_offset.close();

        ifstream chunkbin(chunk_bin_name, ios::binary);

        if(!chunkbin.is_open()){
            cerr << "Chunk Bin" << i << " did not open";
            return 1;
        }

        final_bin << chunkbin.rdbuf();
        chunkbin.close();

        cout << "----- Chunk " << i << " finished combining -----" << endl;

    }

    final_bin.close();
    final_offset.close();

    return 0;
}