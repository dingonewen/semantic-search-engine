#include "StaticFile.hpp"

#include <fstream>

// helper functions

// Takes a file path p and returns the full file contents as a std::string
static std::string ReadAll(const std::string& p) {
  // Opens the file at path p for reading in binary mode so that bytes are read
  // exactily as-is
  std::ifstream in(p, std::ios::binary);

  std::string res;
  char c;
  while (in.get(c)) {
    res += c;
  }
  return res;
}

// Tells the browser how to interprete the bytes it receives
static std::string ContentTypeFor()