// Utils.hpp
// Small utility functions used across the project.

#pragma once

#include <string>
#include <vector>

namespace utils {
// Lowercase a string (ASCII)
std::string to_lower(std::string s);

// Split a file content into words per project definition
std::vector<std::string> tokenize_words(const std::string& content);
}  // namespace utils
