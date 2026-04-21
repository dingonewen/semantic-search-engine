#include "./HttpServer.hpp"
#include "./InvertedIndex.hpp" // ClientCtx
#include <fstream>
#include <iostream>
#include <stdlib.h> // EXIT_SUCCESS
#include <string.h>  // strerror()

#include <arpa/inet.h>  // socket()
#include <unistd.h> // close socket fd



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

// bundles everything the worker thread needs to handle one client connection
struct ClientCtx {   
    int client_fd;
    std::string home_page;
    std::string files_root;
    InvertedIndex* index;  // pointer to the search index for queries
};

// each worker thread runs for one client
/* 
1. Reads the HTTP request from client_fd
2. Calls parse_request()
3. Routes to the right response (/, /query, /static/)
4. Sends the response back
5. Closes client_fd and deletes ctx when done
*/
static void HandleClient(void* arg) {  
    ClientCtx* ctx = static_cast<ClientCtx*>(arg);
    // TODO: implement
    close(ctx->client_fd);
    delete ctx;
}

/*
run() is the main server loop. Main steps are:
1. Load home page — read initial_response_path into a string 
2. Create socket — open a TCP socket, set SO_REUSEADDR, bind to m_port, call listen()
3. Print "accepting connections..."
4. Accept loop — forever call accept(), get a client fd, print the client's IP address
5. Dispatch — for each accepted client, send it to m_pool as a task
6. Client handler (runs in worker thread):
    Read raw bytes until \r\n\r\n
    Call parse_request()
    Route: GET / → home page, GET /query → search results, GET /static/ → file, else → 404
    Send response via make_response()
    Check Connection: close → close socket
7. SIGINT — when Ctrl+C is pressed, break the accept loop and clean up
*/
int HttpServer::run(const std::string& initial_response_path) {
  std::ifstream file(initial_response_path);  
  if (!file.is_open()) {
    std::cerr << "Failed to open: " << initial_response_path << std::endl;
    return EXIT_FAILURE;  // exit code on fatal error
  }
  std::string home_page;
  std::string line;
  while (std::getline(file, line)) {
    home_page += line + '\n';
  }   // load homepage done
  // parsing the port is main()'s job
  int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    std::cerr << strerror(errno) << '\n';
    return EXIT_FAILURE;  
  }
  // Only close listen_fd after the loop exits (when SIGINT stops the server)
  // SO_REUSEADDR tells the OS to skip that wait and let you reuse the port immediately
  // bind(), listen() derived from server_accept_rw_close.cpp
  int optval = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  struct sockaddr_in6 address;
  address = (struct sockaddr_in6) {
      .sin6_family = AF_INET6,
      .sin6_port = htons(static_cast<uint16_t>(m_port)),
      .sin6_addr = IN6ADDR_ANY_INIT,
  };
  int res = bind(listen_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
  if (res != 0) {
      std::cerr << strerror(errno) << '\n';
      close(listen_fd);
      return EXIT_FAILURE;
  }
  // SOMAXCONN is how many pending connections to hold before refusing new ones
  if (listen(listen_fd, SOMAXCONN) != 0) {
    std::cerr << strerror(errno) << '\n';
    close(listen_fd);
    return EXIT_FAILURE;
  }
  // accept loop, derived from server_accept_rw_close.cpp
  // accepting a connection from a client and echo it
  while (true) {
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd = accept(listen_fd,
                           reinterpret_cast<struct sockaddr*>(&caddr),
                           &caddr_len);
    if (client_fd < 0) {
        if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
            continue;
        std::cerr << "Failure on accept: " << strerror(errno) << '\n';
        break;
    }
    // print peer address in main
    // prints in run() right after accept() succeedsand before dispatching to the threadpool
    std::array<char, INET6_ADDRSTRLEN> astring{};
    struct sockaddr_in6* in6 = reinterpret_cast<struct sockaddr_in6*>(&caddr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), astring.data(), INET6_ADDRSTRLEN);
    std::cout << "client " << astring.data() << " port "
              << ntohs(in6->sin6_port) << " connected.\n";
    // dispatch to threadpool
    m_pool.dispatch({HandleClient, new ClientCtx{client_fd, home_page, m_files_root, &m_index}});
    // dispatch call packages HandleClient and a ClientCtx into a Task struct
    // the worker thread calls HandleClient(ctx), which casts ctx back to ClientCtx* and does the work
    ClientCtx* ctx = new ClientCtx{client_fd, home_page, m_files_root, &m_index}; m_pool.dispatch({HandleClient, ctx});
  }
  close(listen_fd);
  return EXIT_FAILURE;
  
}

}  // namespace searchserver