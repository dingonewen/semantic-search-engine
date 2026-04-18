// Index.hpp
// Data structures for indexing words in files and querying them.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Index {
 public:
  Index() = default;

  // Add a word occurrence for filename
  void add_word(const std::string& word, const std::string& filename);

  // Given a set of query words, return list of (filename, rank)
  std::vector<std::pair<std::string, int>> query(
      const std::vector<std::string>& words) const;

 private:
  // word -> filename -> count
  std::unordered_map<std::string, std::unordered_map<std::string, int>> index_;
};
