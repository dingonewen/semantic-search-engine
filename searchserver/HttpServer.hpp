#pragma once
#include <string>
#include "InvertedIndex.hpp"
#include "ThreadPool.hpp"

namespace searchserver {

class HttpServer {
 public:
  HttpServer(int port, const std::string& files_root, size_t num_threads = 8);
  ~HttpServer();

  // run the server (blocking)
  int run(const std::string& initial_response_path);

 private:
  int m_port;
  std::string m_files_root;
  ThreadPool m_pool;
  InvertedIndex m_index;
};

}  // namespace searchserver
