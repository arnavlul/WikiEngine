#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <unordered_set>
#include "json.hpp"
using json = nlohmann::json;

using namespace std;

const string DOC_INFO_PATH = "data_files\\doc_info.jsonl";
const string PAGELINKS_FILE = "data_files\\pagelinks.csv";
const string OUTPUT_FILE = "pagerank_scores.csv";
const int NUM_ITERATIONS = 20;
const double DAMPING_FACTOR = 0.85;

unordered_set<int> valid_pageids;

unordered_map<int, int> real_to_dense;
vector<int> dense_to_real;

vector<vector<int>> inLinks_graph;
vector<int> out_degree;
vector<int> in_degree;

void find_valid_pageids(){
    valid_pageids.reserve(7084107);
    ifstream docinfo(DOC_INFO_PATH);
    if(!docinfo.is_open()){
        cerr << "Error: Could not open docinfo";
        exit(1);
    }

    string line;
    int count=0;
    while(getline(docinfo, line)){
        try{
            auto j = json::parse(line);
            int doc_id = j["id"];
            valid_pageids.insert(doc_id);
        
            count++;
            if(count % 100000 == 0){
                cout << count << " pages added to set...\r" << flush;
            }
        }
        catch(exception &e){
            cout << "Error (while making set): " << e.what() << endl;
            continue;
        }
    }
    cout << endl;
    cout << "Valid pages set complete" << endl;
    return;
}

int map_ids_pass_one(){
    cout << "Pass 1: Discovering unique nodes..." << endl;
    ifstream infile(PAGELINKS_FILE);
    if(!infile.is_open()){
        cerr << "Error: Pagelinks not opening in Pass 1";
        exit(1); 
    }

    string line;
    int u_real, v_real;
    long long line_count = 0;

    while(getline(infile, line)){
        size_t comma_pos = line.find(',');
        if(comma_pos == string::npos) continue;
        try{
            u_real = stoi(line.substr(0, comma_pos));
            v_real = stoi(line.substr(comma_pos+1));

            if((valid_pageids.find(u_real) == valid_pageids.end()) || (valid_pageids.find(v_real) == valid_pageids.end())){
                continue;
            }

            if(real_to_dense.find(u_real) == real_to_dense.end()){
                int new_id = real_to_dense.size();
                real_to_dense[u_real] = new_id;
                dense_to_real.push_back(u_real);
                out_degree.push_back(0);
                in_degree.push_back(0);
            }

            if(real_to_dense.find(v_real) == real_to_dense.end()){
                int new_id = real_to_dense.size();
                real_to_dense[v_real] = new_id;
                dense_to_real.push_back(v_real);
                out_degree.push_back(0);
                in_degree.push_back(0);
            }

            int u_dense = real_to_dense[u_real];
            int v_dense = real_to_dense[v_real];
            
            out_degree[u_dense]++;
            in_degree[v_dense]++;

        }
        catch(exception &e){
            cout << "Error (First Pass): " << e.what() << endl;
            continue;
        }

        line_count++;
        if(line_count % 5000000 == 0){
            cout << "Scanned " << line_count << " lines...\r" << flush;
        }
    }

    infile.close();
    int N = real_to_dense.size();
    cout << "\nTotal unique nodes: " << N << endl;
    return N;
}


void build_graph_pass_two(int N){
    cout << "Pass 2: Building Adjacency List..." << endl;

    cout << "Reserving memory for vectors..." << endl;
    
    inLinks_graph.resize(N);

    for(int i=0; i<N; i++){
        if(in_degree[i] > 0){
            inLinks_graph[i].reserve(in_degree[i]);
        }
    }

    vector<int>().swap(in_degree);
    cout << "  Memory ready. Filling graph..." << endl;

    ifstream infile(PAGELINKS_FILE);
    if(!infile.is_open()){
        cerr << "Error: Pagelinks not opening in Pass 2" << endl;
        exit(1) ;
    }

    string line;
    int u_real, v_real;
    long long line_count = 0;

    while(getline(infile, line)){
        size_t comma_pos = line.find(',');
        if(comma_pos == string::npos) continue;
        try{
            u_real = stoi(line.substr(0, comma_pos));
            v_real = stoi(line.substr(comma_pos + 1));

            if((valid_pageids.find(u_real) == valid_pageids.end()) || (valid_pageids.find(v_real) == valid_pageids.end())){
                continue;
            }

            int u_dense = real_to_dense[u_real];
            int v_dense = real_to_dense[v_real];
            
            inLinks_graph[v_dense].push_back(u_dense);

        }
        catch(exception &e){
            cout << "Error (Second Pass): " << e.what() << endl;
            continue;
        }

        line_count++;
        if(line_count % 5000000 == 0){
            cout << "Loaded " << line_count << " edges...\r" << flush;
        }
    }

    infile.close();

    real_to_dense.clear();
    cout << "\nGraph Loaded." << endl;
}

vector<double> run_pagerank(int N){
    cout << "Calculating pagerank..." << endl;
    if(N<=0) return{};

    vector<double> scores(N, 1.0/N);
    vector<double> new_scores(N, 0.0);

    const double tolerance = 1e-12;

    for(int iteration = 0; iteration < NUM_ITERATIONS; iteration++){
        double diff = 0.0;

        double sink_mass = 0.0;
        for(int j=0; j<N; ++j){
            if(out_degree[j] == 0) sink_mass += scores[j];
        }
      
        const double teleport = (1.0 - DAMPING_FACTOR) / static_cast<double>(N);
        const double sink_contrib = DAMPING_FACTOR * sink_mass / static_cast<double>(N);

        for(int i=0; i<N; i++){
            double sum = 0.0;

            for(int j : inLinks_graph[i]){
                int od = out_degree[j];
                if(od > 0){
                    sum += scores[j] / static_cast<double>(od);
                }
            }
            new_scores[i] = teleport + DAMPING_FACTOR * sum + sink_contrib;
            diff += std::abs(new_scores[i] - scores[i]);
        }

        scores.swap(new_scores);
        std::fill(new_scores.begin(), new_scores.end(), 0.0);

        double avg_diff = diff / static_cast<double>(N);
        cout << "Iteration " << (iteration + 1) << " done. Avg Diff: " << avg_diff << "\r" << flush;

        if (avg_diff < tolerance) {
            cout << "\nConverged in " << (iteration + 1) << " iterations.\n";
            break;
        }
        cout << endl;

    }
    double s = 0.0;
    for(auto sc : scores){
        s += sc;
    }

    if(s>0){
        for(auto& v : scores){
            v /= s;
        }
    }

    return scores;
}

void save_results(const vector<double>& scores, int N){
    cout << "Saving results..." << endl;
    ofstream outfile(OUTPUT_FILE);
    if(!outfile.is_open()){
        cerr << "Could not open output file";
        exit(1);
    }

    outfile << scientific;

    for(int i=0; i<N; i++){
        outfile << dense_to_real[i] << "," << scores[i] << "\n";

        if(i % 10000 == 0){
            cout << i << " scores saved\r" << flush;
        }
    }

    outfile.close();
    cout << "Done! Saved to " << OUTPUT_FILE << endl;

}


int main(){

    cout << "----- Pagerank Starting -----" << endl;

    find_valid_pageids();

    int N = map_ids_pass_one();

    build_graph_pass_two(N);

    vector<double> final_scores = run_pagerank(N);

    save_results(final_scores, N);


    return 0;
}