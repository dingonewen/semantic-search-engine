#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class InvertedIndex {
 public:
  void build(const std::string& root);
  std::vector<std::pair<std::string, int>> search_and_rank(
      const std::vector<std::string>& terms) const;

 private:
  std::unordered_map<std::string, std::unordered_map<std::string, int>> idx;
};
