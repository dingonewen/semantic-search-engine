#include "./HttpServer.hpp"


HttpServer::HttpServer(int port, const std::string& files_root, size_t num_threads = 8) {

}

HttpServer::~HttpServer() {

}

// Starts the server accept loop (blocking — does not return until the process
// is killed). Loads the initial HTML response from initial_response_path and
// serves it at "/". Returns a non-zero exit code on fatal error.
int HttpServer::run(const std::string& initial_response_path) {
    
}