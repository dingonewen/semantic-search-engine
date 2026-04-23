#include "HttpServer.hpp"

#include <array>
#include <climits>     // PATH_MAX - the maximum length of a file path
#include <filesystem>  // read all the files the search server indexes
#include <fstream>
#include <iostream>
#include <string>

using namespace searchserver;

namespace {
// functions in this cpp are not unit testable, so no test cases
// FileExistsReadable and FindInitialResponse are in the namespace
// which gives them internal linkage like static
// these two functions are for finding initial_response.txt when you
// run the server from a different directory than where sample_http/ lives.

// checks if a file exists and can be opened for reading
auto FileExistsReadable(const std::string& p) -> bool {
  std::ifstream in(p);
  return in.good();
}

// try three locations in order to find the home page HTML
auto FindInitialResponse(int argc, char** argv) -> std::string {
  // argv[0] = ./searchserver
  // argv[1] = the port
  // argv[2] = the files_root
  if (argc >= 3) {
    const std::string cand =
        std::string(argv[2]) + "/sample_http/initial_response.txt";
    if (FileExistsReadable(cand)) {
      return cand;
    }
  }
  std::array<char, PATH_MAX> exepath{};
  const ssize_t len =
      // readlink is a Linux system call that reads the target of a symbolic
      // link
      readlink("/proc/self/exe", exepath.data(), exepath.size() - 1);
  if (len > 0) {
    const std::string full(exepath.data(), static_cast<size_t>(len));
    const auto pos = full.find_last_of('/');
    if (pos != std::string::npos) {
      const std::string cand =
          full.substr(0, pos) + "/sample_http/initial_response.txt";
      if (FileExistsReadable(cand)) {
        return cand;
      }
    }
  }
  if (FileExistsReadable("sample_http/initial_response.txt")) {
    return "sample_http/initial_response.txt";
  }
  return {};
}

}  // namespace

/*
1. Validates command line arguments (port, files_root)
2. Finds the initial HTML response file
3. Constructs an HttpServer and calls Run() to start the server
*/
int main(int argc, char** argv) {
  // ./searchserver <port> <search_files>
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <port> <files_root>\n";
    return EXIT_FAILURE;
  }
  // validate port: must be a number >= 1024
  int port = 0;
  try {
    port = std::stoi(argv[1]);
  } catch (...) {
    std::cerr << "Error: port must be a valid integer\n";
    return EXIT_FAILURE;
  }
  if (port < 1024) {
    std::cerr << "Error: port must be >= 1024\n";
    return EXIT_FAILURE;
  }
  // validate files_root: must be an existing directory
  const std::string files_root = argv[2];
  if (!std::filesystem::is_directory(files_root)) {
    std::cerr << "Error: " << files_root << " is not a valid directory\n";
    return EXIT_FAILURE;
  }
  const std::string initial = FindInitialResponse(argc, argv);
  HttpServer srv(port, files_root, 8);
  return srv.Run(initial);
}
