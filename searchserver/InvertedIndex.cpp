#include "InvertedIndex.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::filesystem;

// Split a string into a vector of string using std::isalnum
// delimiter: any charactor that is not alphanumeric
// input: "Hello, world!"
// output: ["hello", "world"]
auto InvertedIndex::Tokenize(const std::string& s) -> std::vector<std::string> {
  std::vector<std::string> res;
  std::string curr;
  // scan the input string char by char
  for (const char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) != 0) {
      // search is case insensitive
      curr.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    } else {
      if (!curr.empty()) {
        res.push_back(curr);
        curr.clear();
      }
    }
  }
  if (!curr.empty()) {
    res.push_back(curr);
  }
  return res;
}

void InvertedIndex::Build(const std::string& root) {
  m_count.clear();
  for (const auto& p : recursive_directory_iterator(root)) {
    // skip folder and symlink
    if (!p.is_regular_file()) {
      continue;
    }
    const std::string path = p.path().string();
    std::ifstream in(path);
    if (!in) {
      continue;
    }
    std::string line;
    while (std::getline(in, line)) {
      auto tokens = Tokenize(line);
      for (auto& token : tokens) {
        m_count[token][path]++;
      }
    }
  }
}

void InvertedIndex::RemoveFile(const std::string& path) {
  for (auto it = m_count.begin(); it != m_count.end();) {
    it->second.erase(path);
    // erase empty entries such as "world": {}
    if (it->second.empty()) {
      it = m_count.erase(it);
    } else {
      ++it;
    }
  }
}

void InvertedIndex::AddFile(const std::string& path) {
  RemoveFile(path);  // re-index: clear old entries first
  std::ifstream in(path);
  if (!in) {
    return;
  }
  // process file and add entry to m_count
  std::string line;
  while (std::getline(in, line)) {
    auto tokens = Tokenize(line);
    for (auto& token : tokens) {
      m_count[token][path]++;
    }
  }
}

auto InvertedIndex::SearchAndRank(const std::vector<std::string>& terms) const
    -> std::vector<std::pair<std::string, int>> {
  // map file to frequency: the number of terms appeared in each file
  std::unordered_map<std::string, int> scores;
  for (const auto& term : terms) {
    auto it = m_count.find(term);
    // the term did not appear in any of the files
    if (it == m_count.end()) {
      continue;
    }
    for (const auto& map : it->second) {
      scores[map.first] += map.second;
    }
  }
  // convert map to vector of pair in order to sort by value
  std::vector<std::pair<std::string, int>> res(scores.begin(), scores.end());
  // sort by value in descending order
  std::ranges::sort(
      res, [](const auto& a, const auto& b) { return a.second > b.second; });
  return res;
}