#include "HttpServer.hpp"

#include <climits>
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
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <port> [files_root]\n";
    return 1;
  }
  const int port = std::stoi(argv[1]);
  std::string files_root;
  if (argc >= 3) {
    files_root = argv[2];
  }

  const std::string initial = find_initial_response(argc, argv);
  HttpServer srv(port, files_root, 8);
  return srv.Run(initial);
}
