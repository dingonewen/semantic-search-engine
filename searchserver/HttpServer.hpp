#ifndef HTTPSERVER_HPP_
#define HTTPSERVER_HPP_

#include <cstddef>
#include <shared_mutex>
#include <string>

#include "InvertedIndex.hpp"
#include "ThreadPool.hpp"

namespace searchserver {

// Manages the lifecycle of the HTTP server: binds a TCP socket, accepts
// incoming connections, and dispatches each connection to a ThreadPool worker.
// Owns an InvertedIndex built from files_root at startup.
class HttpServer {
 public:
  // Constructs the server.
  //   port        -- TCP port to listen on
  //   files_root  -- root directory to serve static files from and index
  //   num_threads -- number of worker threads in the ThreadPool
  HttpServer(int port, std::string files_root, size_t num_threads);

  ~HttpServer() = default;

  HttpServer(const HttpServer&) = delete;
  HttpServer(HttpServer&&) = delete;
  auto operator=(const HttpServer&) -> HttpServer& = delete;
  auto operator=(HttpServer&&) -> HttpServer& = delete;

  // Starts the server accept loop (blocking — does not return until the process
  // is killed). Loads the initial HTML response from initial_response_path and
  // serves it at "/". Returns a non-zero exit code on fatal error.
  auto Run(const std::string& initial_response_path) -> int;

 private:
  int m_port;
  std::string m_files_root;
  InvertedIndex m_index;        // Full-text index built from m_files_root
  std::shared_mutex m_index_mtx;  // guards m_index for concurrent read/write
  ThreadPool m_pool;            // declared last: destroyed first, joining threads before m_index
};

}  // namespace searchserver

#endif  // HTTPSERVER_HPP_
