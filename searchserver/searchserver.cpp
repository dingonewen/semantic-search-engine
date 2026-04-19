#include "HttpServer.hpp"
#include <iostream>
#include <string>
#include <unistd.h>
#include <limits.h>
#include <fstream>
#include <filesystem>

using namespace searchserver;

static bool file_exists_readable(const std::string &p) {
  std::ifstream in(p);
  return in.good();
}

static std::string find_initial_response(int argc, char **argv) {
  // 1) if argv[2] provided, check <arg>/sample_http/initial_response.txt
  if (argc >= 3) {
    std::string cand = std::string(argv[2]) + "/sample_http/initial_response.txt";
    if (file_exists_readable(cand)) return cand;
  }
  // 2) exe dir
  char exepath[PATH_MAX] = {0};
  ssize_t len = readlink("/proc/self/exe", exepath, sizeof(exepath)-1);
  if (len > 0) {
    std::string full(exepath, static_cast<size_t>(len));
    auto pos = full.find_last_of('/');
    if (pos != std::string::npos) {
      std::string dir = full.substr(0,pos);
      std::string cand = dir + "/sample_http/initial_response.txt";
      if (file_exists_readable(cand)) return cand;
    }
  }
  // 3) cwd
  if (file_exists_readable("sample_http/initial_response.txt")) return "sample_http/initial_response.txt";
  return {};
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <port> [files_root]\n";
    return 1;
  }
  int port = std::stoi(argv[1]);
  std::string files_root;
  if (argc >= 3) files_root = argv[2];

  std::string initial = find_initial_response(argc, argv);
  HttpServer srv(port, files_root, 8);
  return srv.run(initial);
}
