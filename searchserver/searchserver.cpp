#include "HttpServer.hpp"

#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace searchserver;

namespace {

auto file_exists_readable(const std::string& p) -> bool {
  std::ifstream in(p);
  return in.good();
}

auto find_initial_response(int argc, char** argv) -> std::string {
  if (argc >= 3) {
    const std::string cand =
        std::string(argv[2]) + "/sample_http/initial_response.txt";
    if (file_exists_readable(cand)) {
      return cand;
    }
  }
  std::array<char, PATH_MAX> exepath{};
  const ssize_t len =
      readlink("/proc/self/exe", exepath.data(), exepath.size() - 1);
  if (len > 0) {
    const std::string full(exepath.data(), static_cast<size_t>(len));
    const auto pos = full.find_last_of('/');
    if (pos != std::string::npos) {
      const std::string cand =
          full.substr(0, pos) + "/sample_http/initial_response.txt";
      if (file_exists_readable(cand)) {
        return cand;
      }
    }
  }
  if (file_exists_readable("sample_http/initial_response.txt")) {
    return "sample_http/initial_response.txt";
  }
  return {};
}

}  // namespace

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

  const std::string initial = find_initial_response(argc, argv);
  HttpServer srv(port, files_root, 8);
  return srv.Run(initial);
}
