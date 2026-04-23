#include "HttpServer.hpp"

#include <filesystem>
#include <iostream>
#include <string>

using namespace searchserver;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <port> <files_root>\n";
    return 1;
  }

  // validate port: must be a number >= 1024
  int port = 0;
  try {
    port = std::stoi(argv[1]);
  } catch (...) {
    std::cerr << "Error: port must be a valid integer\n";
    return 1;
  }
  if (port < 1024) {
    std::cerr << "Error: port must be >= 1024\n";
    return 1;
  }

  // validate files_root: must be an existing directory
  const std::string files_root = argv[2];
  if (!std::filesystem::is_directory(files_root)) {
    std::cerr << "Error: " << files_root << " is not a valid directory\n";
    return 1;
  }

  HttpServer srv(port, files_root, 8);
  return srv.Run("sample_http/initial_response.txt");
}
