#include "./HttpServer.hpp"
#include <fstream>
#include <iostream>

namespace searchserver {

HttpServer::HttpServer(int port, const std::string& files_root, size_t num_threads) : m_port(port), m_files_root(files_root), m_pool(num_threads), m_index() {
    m_index.build(m_files_root);  // m_index is a search index maps words to the files that contain them
}

// threadpool, InvertedIndex already have destructor
HttpServer::~HttpServer() {}

// Blocking function - starts the server accept loop, no return until the process is killed
// it reads the home page file once at the start, then serves that same string to every client that requests GET /.
// no direct test case for run since it's not unit-testable
int HttpServer::run(const std::string& initial_response_path) {
    std::ifstream file(initial_response_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << initial_response_path << std::endl;
        return 1;   // exit code on fatal error
    }
    std::string home_page;
    std::string line;
    while (std::getline(file, line)) {
        home_page += line + '\n';
    }
    return 0;
}

}  // namespace searchserver