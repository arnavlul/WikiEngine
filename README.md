# WikiEngine 🚀

A high-performance, from-scratch search engine capable of indexing and querying millions of Wikipedia articles. This project implements advanced data structures and ranking algorithms in C++ to provide a complete search experience with real-time suggestions and authority-based ranking.

## ✨ Key Features

- **Custom Search Engine:** Built from the ground up in C++ without external search libraries (like Lucene or Whoosh).
- **Hybrid Ranking:** Combines **BM25 (Best Matching 25)** for content relevance and **PageRank** for link-based authority.
- **Real-Time Autocomplete:** An optimized **Trie (Prefix Tree)** structure that provides sub-millisecond query suggestions.
- **Distributed Indexing:** A multi-step "MapReduce-style" pipeline that shards raw data, builds partial indices, and merges them into a single binary index.
- **Modern GUI:** A dedicated desktop application built with **Qt 6** for an intuitive search experience.
- **Cross-Language Stemming:** Integrated Python-based **Porter Stemmer** for accurate word normalization.

## 🏗️ System Architecture

The engine is divided into several specialized components:

### 1. The Indexing Pipeline
- `splitter.cpp`: Shards raw Wikipedia JSONL data into manageable chunks based on term hashes.
- `indexer_shard.cpp`: Processes individual shards to create partial inverted indices.
- `mergebins.cpp`: Merges all partial indices into a final, high-performance `index.bin`.

### 2. Authority & Relevance
- `pageRank.cpp`: Processes millions of Wikipedia page links using the iterative PageRank algorithm to assign "authority" scores to every page.
- `search.cpp`: The core query processor. It calculates BM25 scores and combines them with PageRank values for the final result list.

### 3. Real-Time Suggestions
- `autoCompleteTrie.cpp`: Builds a binary-serialized Trie structure from the entire Wikipedia title set, enabling instant "search-as-you-type" functionality.

## 🛠️ Technical Stack

- **Primary Language:** C++17/20
- **Interface:** Qt 6.9 Framework
- **Data Processing:** Python (NLTK for stemming, BeautifulSoup for parsing)
- **Data Formats:** Binary (Custom serialization), JSONL, CSV
- **Algorithms:** BM25, PageRank, Trie DFS, Hash-based Sharding

## 🚀 Getting Started

### Prerequisites
- C++ Compiler (GCC/Clang/MinGW)
- Python 3.x (with `nltk` installed)
- Qt 6 (for the GUI)

### Build & Run
1. **Build the Index:**
   ```bash
   # Run the indexing pipeline
   ./splitter
   ./indexer_shard <shard_id>
   ./mergebins
   ```
2. **Build the Autocomplete Trie:**
   ```bash
   ./autoCompleteTrie build
   ```
3. **Run the Search Console:**
   ```bash
   ./search
   ```
4. **Launch the GUI:** Open `WikiEngineGUI/CMakeLists.txt` in Qt Creator and build.

## 📁 Project Structure

```text
├── WikiEngineGUI/      # Qt-based Desktop Application
├── BinsAndTxtx/        # Final binary index files
├── data_files/         # Raw and processed Wikipedia dumps
├── Parsers/            # Python scripts for data preparation
├── search.cpp          # Main query engine logic
├── pageRank.cpp        # Link authority calculator
└── autoCompleteTrie.cpp # Real-time prefix suggestions
```
