#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// Maps words to the files that contain them and their term frequencies.
// Supports building from a directory, incremental updates, and ranked search.
class InvertedIndex {
 public:
  // Split a string into a vector of string using std::isalnum
  // delimiter: any charactor that is not alphanumeric
  static std::vector<std::string> tokenize(const std::string& s);

  // Recursively reads all files under root and indexes every word found.
  // Input:  root -- path to the directory to index (e.g. "test_tree")
  void build(const std::string& root);

  // Removes all index entries associated with the given file.
  // Input:  path -- absolute path of the file to de-index
  void remove_file(const std::string& path);

  // Indexes (or re-indexes) a single file, adding its words to the index.
  // Input:  path -- absolute path of the file to index
  void add_file(const std::string& path);

  // Searches the index for all given terms and ranks results by total
  // term-frequency score (sum of per-term counts across all matching files).
  // Input:  terms  -- list of lowercase search terms
  // Output: vector of (relative filename, score) pairs, sorted by score desc.
  std::vector<std::pair<std::string, int>> search_and_rank(
      const std::vector<std::string>& terms) const;

 private:
  // m_count[word][filepath] = count of 'word' in 'filepath'
  std::unordered_map<std::string, std::unordered_map<std::string, int>> m_count;
};
