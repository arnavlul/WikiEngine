#include "porterStemmer.hpp"
#include <iostream>
#include <string>
#include <algorithm>
using namespace std;

int main(int argc, char* argv[]){

    string raw_term;
    if(argc < 2){
        cout << "Word: ";
        cin >> raw_term;
    }
    else{
        raw_term = argv[1];
    }
    PorterStemmer stemmer;


    transform(raw_term.begin(), raw_term.end(), raw_term.begin(), ::tolower);
    string stemmed = stemmer.stem(raw_term);

    cout << "Original: " << raw_term << ", Stemmed: " << stemmed << endl;
;
    return 0;
}