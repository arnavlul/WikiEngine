#include <iostream>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

using namespace std;

// Function to execute a command and get the output
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int argc, char* argv[]){

    string raw_term;
    if(argc < 2){
        cout << "Word: ";
        cin >> raw_term;
    }
    else{
        // FIX: Use argv, not argc
        raw_term = argv[1];
    }

    // Prepare the command to call the python script
    // Make sure stemmer_bridge.py is in the same directory
    string command = "python stemmer_bridge.py " + raw_term;
    
    string stemmed = "";
    try {
        stemmed = exec(command.c_str());
    } catch (const std::exception& e) {
        cerr << "Error calling python script: " << e.what() << endl;
        return 1;
    }

    transform(raw_term.begin(), raw_term.end(), raw_term.begin(), ::tolower);
    // We rely on python to stem, but we lowercase first for display consistency if needed
    
    cout << "Original: " << raw_term << endl;
    cout << "Stemmed (Python): " << stemmed << endl;

    return 0;
}