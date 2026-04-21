#include "StaticFile.hpp"

#include <filesystem>
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
static std::string ContentTypeFor(const std::string& p) {
  // broswer displays raw text
  if (p.size() >= 4 && p.substr(p.size() - 4) == ".txt")
    return "text/plain";
  // broswer renders it as a webpage
  if (p.size() >= 5 && p.substr(p.size() - 5) == ".html")
    return "text/html";
  // browser treats it as a generic binary download
  return "application/octet-stream";
}

std::string StaticGet(const std::string& files_root,
                      const std::string& relpath) {
  std::string path = files_root + "/" + relpath;
  if (std::filesystem::is_regular_file(path)) {
    return MakeResponse(200, ReadAll(path), ContentTypeFor(path));
  }
  // not found
  return MakeResponse(404, "<h1>404 Not Found</h1>", "test/html");
}

std::string StaticPut()