#include "InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std::filesystem;

static std::vector<std::string> tokenize(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (std::isalnum((unsigned char)c))
      cur.push_back(static_cast<char>(std::tolower((unsigned char)c)));
    else {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    }
  }
  if (!cur.empty())
    out.push_back(cur);
  return out;
}

void InvertedIndex::build(const std::string& root) {
  idx.clear();
  for (auto& p : recursive_directory_iterator(root)) {
    if (!p.is_regular_file())
      continue;
    std::string path = p.path().string();
    std::ifstream in(path);
    if (!in)
      continue;
    std::string line;
    while (std::getline(in, line)) {
      auto toks = tokenize(line);
      for (auto& t : toks)
        idx[t][path]++;
    }
  }
}

std::vector<std::pair<std::string, int>> InvertedIndex::search_and_rank(
    const std::vector<std::string>& terms) const {
  std::unordered_map<std::string, int> scores;
  for (auto& t : terms) {
    auto it = idx.find(t);
    if (it == idx.end())
      continue;
    for (auto& p : it->second)
      scores[p.first] += p.second;
  }
  std::vector<std::pair<std::string, int>> out(scores.begin(), scores.end());
  std::sort(out.begin(), out.end(),
            [](auto& a, auto& b) { return a.second > b.second; });
  return out;
}
