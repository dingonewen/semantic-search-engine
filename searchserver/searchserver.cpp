#include "HttpServer.hpp"

#include <filesystem>
#include <iostream>
#include <string>

using namespace searchserver;

/*
1. Validates command line arguments (port, files_root)
2. Constructs an HttpServer and calls Run() to start the server
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
  const size_t hw = std::thread::hardware_concurrency();
  HttpServer srv(port, files_root, hw != 0 ? hw : 8);
  return srv.Run("sample_http/initial_response.txt");
}
