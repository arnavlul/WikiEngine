#ifndef CHECK_STEM_HPP
#define CHECK_STEM_HPP

#include <iostream>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

using namespace std;

class PythonStemmer {
public:
    PythonStemmer(const string& scriptPath = "stemmer_bridge.py") : scriptPath(scriptPath) {}

    string stem(const string& raw_term) {
        // Prepare the command to call the python script
        // Make sure stemmer_bridge.py is in the same directory or provide full path
        string command = "python " + scriptPath + " " + raw_term;
        
        string stemmed = "";
        try {
            stemmed = exec(command.c_str());
        } catch (const std::exception& e) {
            cerr << "Error calling python script: " << e.what() << endl;
            return raw_term; // Return original if stemming fails
        }
        
        // Optional: trim newline if python script output one (though our script uses end='')
        if (!stemmed.empty() && stemmed.back() == '\n') {
            stemmed.pop_back();
        }

        return stemmed;
    }

private:
    string scriptPath;

    // Function to execute a command and get the output
    string exec(const char* cmd) {
        array<char, 128> buffer;
        string result;
        // Use _popen on Windows, popen on Linux/Mac
        #ifdef _WIN32
            unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
        #else
            unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        #endif

        if (!pipe) {
            throw runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }
};

#endif // CHECK_STEM_HPP