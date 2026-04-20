#include "./HttpServer.hpp"
#include <fstream>
#include <iostream>

namespace searchserver {

HttpServer::HttpServer(int port,
                       const std::string& files_root,
                       size_t num_threads)
    : m_port(port), m_files_root(files_root), m_pool(num_threads), m_index() {
  m_index.build(m_files_root);  // m_index is a search index maps words to the
                                // files that contain them
}

// threadpool, InvertedIndex already have destructor
HttpServer::~HttpServer() {}

/*
run() is the main server loop. In order:

Load home page — read initial_response_path into a string 
Create socket — open a TCP socket, set SO_REUSEADDR, bind to m_port, call listen()
Print "accepting connections..."
Accept loop — forever call accept(), get a client fd, print the client's IP address
Dispatch — for each accepted client, send it to m_pool as a task
Client handler (runs in worker thread):
Read raw bytes until \r\n\r\n
Call parse_request()
Route: GET / → home page, GET /query → search results, GET /static/ → file, else → 404
Send response via make_response()
Check Connection: close → close socket
SIGINT — when Ctrl+C is pressed, break the accept loop and clean up
*/
int HttpServer::run(const std::string& initial_response_path) {
  std::ifstream file(initial_response_path);  
  if (!file.is_open()) {
    std::cerr << "Failed to open: " << initial_response_path << std::endl;
    return 1;  // exit code on fatal error
  }
  std::string home_page;
  std::string line;
  while (std::getline(file, line)) {
    home_page += line + '\n';
  }   // load homepage done
  return 0;
}

}  // namespace searchserver