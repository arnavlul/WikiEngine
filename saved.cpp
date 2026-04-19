#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iomanip>

using namespace std;

// --- CONFIGURATION ---
const string PAGELINKS_FILE = "pagelinks.csv";
const string OUTPUT_FILE = "pagerank_scores.csv";
const int NUM_ITERATIONS = 20;
const double DAMPING_FACTOR = 0.85;

// --- GLOBAL DATA STRUCTURES ---
// Map Real Wikipedia IDs -> Dense IDs (0 to N-1)
unordered_map<int, int> real_to_dense;
vector<int> dense_to_real;

// The Graph: adj[u] = list of nodes v such that v -> u (Incoming Links)
vector<vector<int>> incoming_links;
vector<int> out_degree;
vector<int> in_degree; // Only needed for memory reservation phase

// --- FUNCTIONS ---

int map_ids_pass_one() {
    cout << "Pass 1: Discovering unique nodes and counting degrees..." << endl;
    ifstream infile(PAGELINKS_FILE);
    if (!infile.is_open()) { 
        cerr << "Error opening file: " << PAGELINKS_FILE << endl; 
        exit(1); 
    }

    string line;
    int u_real, v_real;
    long long line_count = 0;

    while (getline(infile, line)) {
        size_t comma_pos = line.find(',');
        if (comma_pos == string::npos) continue;
        try {
            u_real = stoi(line.substr(0, comma_pos));
            v_real = stoi(line.substr(comma_pos + 1));

            // Insert Source into map if new
            if (real_to_dense.find(u_real) == real_to_dense.end()) {
                int new_id = real_to_dense.size();
                real_to_dense[u_real] = new_id;
                dense_to_real.push_back(u_real);
                out_degree.push_back(0); // Initialize count
                in_degree.push_back(0);  // Initialize count
            }
            // Insert Target into map if new
            if (real_to_dense.find(v_real) == real_to_dense.end()) {
                int new_id = real_to_dense.size();
                real_to_dense[v_real] = new_id;
                dense_to_real.push_back(v_real);
                out_degree.push_back(0); // Initialize count
                in_degree.push_back(0);  // Initialize count
            }

            // Count Degrees
            int u_dense = real_to_dense[u_real];
            int v_dense = real_to_dense[v_real];

            out_degree[u_dense]++; // u links OUT to v
            in_degree[v_dense]++;  // v has IN link from u

        } catch (...) { continue; }
        
        line_count++;
        if (line_count % 5000000 == 0) cout << "Scanned " << line_count << " lines...\r" << flush;
    }
    infile.close();
    
    int N = real_to_dense.size();
    cout << "\nTotal Unique Nodes: " << N << endl;
    return N;
}

void build_graph_pass_two(int N) {
    cout << "Pass 2: Building Adjacency List..." << endl;
    
    // 1. Allocate Exact Memory
    cout << "  Reserving memory for vectors..." << endl;
    incoming_links.resize(N);
    
    for (int i = 0; i < N; i++) {
        if (in_degree[i] > 0) {
            // Critical Optimization: Reserve exact space to prevent reallocations
            incoming_links[i].reserve(in_degree[i]);
        }
    }
    
    // Free in_degree memory now (we don't need it for PageRank algorithm itself)
    vector<int>().swap(in_degree);
    cout << "  Memory ready. Filling graph..." << endl;
    
    // 2. Fill Graph
    ifstream infile(PAGELINKS_FILE);
    if (!infile.is_open()) {
        cerr << "Error reopening file for Pass 2." << endl;
        exit(1);
    }

    string line;
    int u_real, v_real;
    long long line_count = 0;

    while (getline(infile, line)) {
        size_t comma_pos = line.find(',');
        if (comma_pos == string::npos) continue;
        try {
            u_real = stoi(line.substr(0, comma_pos));
            v_real = stoi(line.substr(comma_pos + 1));
            
            // IDs definitely exist now, direct map lookup is safe
            // Using .at() or [] assumes valid keys, which is guaranteed by Pass 1
            int u_dense = real_to_dense[u_real];
            int v_dense = real_to_dense[v_real];

            // Store incoming link: v <-- u
            incoming_links[v_dense].push_back(u_dense);
            // We do NOT increment out_degree here, we already did it in Pass 1
            
        } catch (...) {}
        
        line_count++;
        if (line_count % 5000000 == 0) cout << "Loaded " << line_count << " edges...\r" << flush;
    }
    infile.close();
    
    // Free the map to reclaim ~1GB RAM
    real_to_dense.clear(); 
    cout << "\nGraph loaded." << endl;
}

vector<double> run_pagerank(int N) {
    cout << "Calculating PageRank..." << endl;
    vector<double> scores(N, 1.0 / N);
    vector<double> new_scores(N);
    
    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        double diff = 0.0;
        
        for (int i = 0; i < N; i++) {
            double sum = 0.0;
            
            // Sum contribution from all nodes 'j' that point to 'i'
            for (int j : incoming_links[i]) {
                if (out_degree[j] > 0) {
                    sum += scores[j] / out_degree[j];
                }
            }
            
            new_scores[i] = (1.0 - DAMPING_FACTOR) + (DAMPING_FACTOR * sum);
            diff += abs(new_scores[i] - scores[i]);
        }
        
        scores = new_scores;
        cout << "Iteration " << (iter + 1) << " done. Avg Diff: " << (diff / N) << "\r" << flush;
    }
    cout << endl;
    return scores;
}

void save_results(const vector<double>& scores, int N) {
    cout << "Saving results..." << endl;
    ofstream outfile(OUTPUT_FILE);
    if (!outfile.is_open()) {
        cerr << "Error creating output file." << endl;
        return;
    }

    outfile << "doc_id,score\n";
    outfile << fixed << setprecision(6);
    
    for (int i = 0; i < N; i++) {
        // Map dense ID back to Real ID for final output
        outfile << dense_to_real[i] << "," << scores[i] << "\n";
    }
    outfile.close();
    cout << "Done! Saved to " << OUTPUT_FILE << endl;
}

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    cout << "--- Memory-Efficient PageRank ---" << endl;

    // 1. Map IDs and Count Degrees (Pass 1)
    int N = map_ids_pass_one();

    // 2. Allocate & Build Graph (Pass 2)
    build_graph_pass_two(N);

    // 3. Run Algorithm
    vector<double> final_scores = run_pagerank(N);

    // 4. Save
    save_results(final_scores, N);

    return 0;
}