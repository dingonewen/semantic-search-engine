// FileCrawler.hpp
// Responsible for crawling a directory tree and producing file lists/contents.

#pragma once

#include <string>
#include <vector>

class FileCrawler {
 public:
  explicit FileCrawler(std::string root_directory);

  // Crawl and return list of file paths (absolute or relative to root)
  std::vector<std::string> crawl() const;

  // Read file contents; throws on error or returns empty string if not found
  std::string read_file(const std::string& path) const;

 private:
  std::string root_;
};
