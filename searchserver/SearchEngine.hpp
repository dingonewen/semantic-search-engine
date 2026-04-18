// SearchEngine.hpp
// High-level API that uses FileCrawler and Index to build and answer queries.

#pragma once

#include <string>
#include <utility>
#include <vector>

class SearchEngine {
 public:
  // Build index from path; may be expensive
  explicit SearchEngine(std::string root_path);

  // Rebuild or update index
  void build_index();

  // Query the index
  std::vector<std::pair<std::string, int>> search(
      const std::vector<std::string>& words) const;

 private:
  std::string root_;
};
