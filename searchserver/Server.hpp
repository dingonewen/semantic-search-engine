// Server.hpp
// Main server loop which accepts connections and dispatches to worker threads.

#pragma once

#include <string>

class ThreadPool;  // forward declare
class SearchEngine;

class Server {
 public:
  Server(uint16_t port, const std::string& root_path, size_t nthreads = 4);
  ~Server();

  // Start accepting connections; this call blocks until stopped
  void run();

  // Stop the server gracefully
  void stop();

 private:
  uint16_t port_;
  std::string root_;
  size_t nthreads_;
};
