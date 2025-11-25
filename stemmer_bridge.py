import sys
from nltk.stem import PorterStemmer

def main():
    if len(sys.argv) < 2:
        return

    word = sys.argv[1]
    porter = PorterStemmer()
    stemmed_word = porter.stem(word)
    print(stemmed_word, end='') # Print without newline for easier C++ reading

if __name__ == "__main__":
    main()