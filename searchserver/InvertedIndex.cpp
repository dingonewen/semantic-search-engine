#include "InvertedIndex.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace std::filesystem;

// input: "Hello, world!"
// output: ["hello", "world"]
std::vector<std::string> InvertedIndex::tokenize(const std::string& s) {
  std::vector<std::string> res;
  std::string curr;
  // scan the input string char by char
  for (char c : s) {
    if (std::isalnum((unsigned char)c)) {
      // search is case insensitive
      curr.push_back(static_cast<char>(std::tolower((unsigned char)c)));
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

void InvertedIndex::build(const std::string& root) {
  m_count.clear();
  for (auto& p : recursive_directory_iterator(root)) {
    // skip folder and symlink
    if (!p.is_regular_file()) {
      continue;
    }
    std::string path = p.path().string();
    std::ifstream in(path);
    if (!in) {
      continue;
    }
    std::string line;
    while (std::getline(in, line)) {
      auto tokens = tokenize(line);
      for (auto& token : tokens) {
        m_count[token][path]++;
      }
    }
  }
}

void InvertedIndex::remove_file(const std::string& path) {
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

void InvertedIndex::add_file(const std::string& path) {
  remove_file(path);  // re-index: clear old entries first
  std::ifstream in(path);
  if (!in) {
    return;
  }
  // process file and add entry to m_count
  std::string line;
  while (std::getline(in, line)) {
    auto tokens = tokenize(line);
    for (auto& token : tokens) {
      m_count[token][path]++;
    }
  }
}

std::vector<std::pair<std::string, int>> InvertedIndex::search_and_rank(
    const std::vector<std::string>& terms) const {
  // map file to frequency: the number of terms appeared in each file
  std::unordered_map<std::string, int> scores;
  for (auto term : terms) {
    auto it = m_count.find(term);
    // the term did not appear in any of the files
    if (it == m_count.end()) {
      continue;
    }
    for (auto& map : it->second) {
      scores[map.first] += map.second;
    }
  }
  // convert map to vector of pair in order to sort by value
  std::vector<std::pair<std::string, int>> res(scores.begin(), scores.end());
  // sort by value in descending order
  std::sort(res.begin(), res.end(),
            [](auto& a, auto& b) { return a.second > b.second; });
  return res;
}