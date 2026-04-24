#include "./HttpServer.hpp"
#include "./HttpRequest.hpp"   // ParseRequest, SplitTerms
#include "./HttpResponse.hpp"  // MakeResponse, k_http_*
#include "./InvertedIndex.hpp"
#include "./StaticFile.hpp"  // serve_static, static_put, static_delete

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

namespace {
volatile sig_atomic_t g_done = 0;  // set to 1 on SIGINT
volatile sig_atomic_t g_listen_fd =
    -1;  // set before accept loop so handler can close it

void SigintHandler(int /*signo*/) {
  g_done = 1;
  // closing listen_fd forces accept() to return immediately with an error,
  // which lets the accept loop re-check g_done and exit cleanly
  const int fd = g_listen_fd;
  if (fd >= 0) {
    g_listen_fd = -1;
    close(fd);
  }
}
}  // namespace

namespace searchserver {

// Internal helpers below are in an anonymous namespace (not unit-testable
// directly). They are covered through integration testing via
// HttpServer::Run().
namespace {

// bundles everything the worker thread needs to handle one client connection
struct ClientCtx {
  int client_fd;
  std::string home_page;
  std::string files_root;
  InvertedIndex* index;  // pointer to the search index for queries
};

/*
HandleClient's inner logic was split into ReadRawRequest, HandleGetRequest,
HandleStaticMutation, and SendAll for tidy-check
*/
// read until \r\n\r\n (end of headers), chunk by chunk
auto ReadRawRequest(int fd) -> std::string {
  std::string raw;
  std::array<char, 4096> buf{};
  while (true) {
    const ssize_t n = read(fd, buf.data(), buf.size() - 1);
    if (n < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        continue;
      }
      break;
    }
    if (n == 0) {
      break;  // client disconnected
    }
    raw.append(buf.data(), static_cast<size_t>(n));
    if (raw.find("\r\n\r\n") != std::string::npos) {
      break;
    }
  }
  return raw;
}

// handles GET requests: route to home page, query results, or static file
auto HandleGetRequest(const Request& r, ClientCtx* ctx) -> std::string {
  if (r.path == "/" || r.path.empty()) {
    return MakeResponse(k_http_ok, ctx->home_page, "text/html");
  }
  if (r.path == "/query") {
    auto terms = SplitTerms(r.query);
    auto results = ctx->index->SearchAndRank(terms);
    std::string body = ctx->home_page;
    std::string links;
    for (const auto& p : results) {
      links += "<li><a href=\"/static/" + p.first + "\">" + p.first + "</a> [" +
               std::to_string(p.second) + "]</li>\n";
    }
    body += "<p>" + std::to_string(results.size()) + " results found</p>\n";
    body += "<ul>\n" + links + "</ul>\n";
    return MakeResponse(k_http_ok, body, "text/html");
  }
  if (r.path.starts_with("/static/")) {
    const std::string rel = r.path.substr(8);
    return StaticGet(ctx->files_root, rel);
  }
  return MakeResponse(k_http_not_found, "<h1>404 Not Found</h1>", "text/html");
}

// handles PUT/POST/DELETE on /static/ paths
auto HandleStaticMutation(const Request& r, ClientCtx* ctx) -> std::string {
  const std::string rel = r.path.substr(8);
  if (r.method == "PUT") {
    return StaticPut(ctx->files_root, rel, r.body, true);  // PUT has body
  }
  if (r.method == "POST") {
    return StaticPut(ctx->files_root, rel, r.body, false);  // POST has body
  }
  if (r.method == "DELETE") {
    return StaticDelete(ctx->files_root, rel);
  }
  return MakeResponse(k_http_not_implemented, "Not Implemented", "text/plain",
                      "Not Implemented");
}

// send response (loop to handle short writes)
void SendAll(int fd, const std::string& response) {
  auto to_send = static_cast<ssize_t>(response.size());
  const char* ptr = response.data();
  while (to_send > 0) {
    const ssize_t w = write(fd, ptr, static_cast<size_t>(to_send));
    if (w <= 0) {
      break;
    }
    to_send -= w;
    ptr += w;
  }
}

// each worker thread runs for one client
/*
1. Reads the HTTP request from client_fd
2. Calls ParseRequest()
3. Routes to the right response (/, /query, /static/)
4. Sends the response back
5. Closes client_fd and deletes ctx when done
*/
// Different from server_accept_rw_close.cpp: until \r\n\r\n → parse → route
// to different handlers → send appropriate response → check Connection: close →
// loop back and read next request
void HandleClient(void* arg) {
  auto* ctx = static_cast<ClientCtx*>(arg);
  // keep-alive loop: each iteration handles one full request-response cycle
  // browser may send multiple requests on same connection before closing
  while (true) {
    const std::string raw = ReadRawRequest(ctx->client_fd);
    if (raw.find("\r\n\r\n") == std::string::npos) {
      break;
    }
    const Request r = ParseRequest(raw);
    // route request to appropriate handler based on method and path
    std::string response;
    if (r.method == "GET") {
      response = HandleGetRequest(r, ctx);
    } else if (r.path.starts_with("/static/")) {
      response = HandleStaticMutation(r, ctx);
    } else {
      response =
          MakeResponse(k_http_not_found, "<h1>404 Not Found</h1>", "text/html");
    }
    SendAll(ctx->client_fd, response);
    // check Connection: close
    const auto it = r.headers.find("connection");
    if (it != r.headers.end() && it->second == "close") {
      break;
    }
  }
  close(ctx->client_fd);
  delete ctx;
}

}  // namespace

HttpServer::HttpServer(int port, std::string files_root, size_t num_threads)
    : m_port(port),
      m_files_root(std::move(files_root)),
      m_pool(num_threads),
      m_index() {
  m_index.Build(m_files_root);  // m_index is a search index maps words to the
                                // files that contain them
}

/*
Run() is the main server loop. Main steps are:
1. Load home page — read initial_response_path into a string
2. Create socket — open a TCP socket, set SO_REUSEADDR, bind to m_port, call
listen()
3. Print "accepting connections..."
4. Accept loop — forever call accept(), get a client fd, print the client's IP
address
5. Dispatch — for each accepted client, send it to m_pool as a task
6. Client handler (runs in worker thread):
    Read raw bytes until \r\n\r\n
    Call ParseRequest()
    Route: GET / → home page, GET /query → search results, GET /static/ → file,
else → 404 Send response via MakeResponse() Check Connection: close → close
socket
7. SIGINT — when Ctrl+C is pressed, break the accept loop and clean up
*/
auto HttpServer::Run(const std::string& initial_response_path) -> int {
  // register SIGINT handler

  struct sigaction sigact{};
  sigact.sa_handler = SigintHandler;
  // remove SA_RESTART flag to continue re-check g_dne
  sigact.sa_flags = 0;
  sigaction(SIGINT, &sigact, nullptr);

  // read home page HTML once at startup — initial_response.txt is a full HTTP
  // response, so strip the headers and keep only the HTML body after \n\n
  std::ifstream file(initial_response_path);
  if (!file.is_open()) {
    std::cerr << "Failed to open: " << initial_response_path << '\n';
    return EXIT_FAILURE;
  }
  std::string home_page;
  std::string line;
  while (std::getline(file, line)) {
    home_page += line + '\n';
  }
  const auto sep = home_page.find("\n\n");
  if (sep != std::string::npos) {
    home_page = home_page.substr(sep + 2);
  }

  // parsing the port is main()'s job
  const int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    std::cerr << strerror(errno) << '\n';
    return EXIT_FAILURE;
  }
  // Only close listen_fd after the loop exits (when SIGINT stops the server)
  // SO_REUSEADDR tells the OS to skip that wait and let you reuse the port
  // immediately bind(), listen() derived from server_accept_rw_close.cpp
  const int optval = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  struct sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(static_cast<uint16_t>(m_port));
  address.sin6_addr = IN6ADDR_ANY_INIT;

  const int res = bind(listen_fd, reinterpret_cast<struct sockaddr*>(&address),
                       sizeof(address));
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
  g_listen_fd = listen_fd;  // expose to signal handler so it can close it
  std::cout << "accepting connections...\n";
  // accept loop, derived from server_accept_rw_close.cpp
  // accepting a connection from a client and echo it
  while (g_done ==
         0) {  // loop exits when Ctrl+C pressed and SIGINT sets g_done = 1
    struct sockaddr_storage caddr{};
    socklen_t caddr_len = sizeof(caddr);
    const int client_fd = accept(
        listen_fd, reinterpret_cast<struct sockaddr*>(&caddr), &caddr_len);
    if (client_fd < 0) {
      if (g_done != 0)
        break;  // SIGINT closed listen_fd, exit cleanly
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        continue;
      }
      std::cerr << "Failure on accept: " << strerror(errno) << '\n';
      break;
    }
    // prints in Run() right after accept() succeeds and before dispatching to
    // the threadpool
    std::array<char, INET6_ADDRSTRLEN> astring{};
    auto* in6 = reinterpret_cast<struct sockaddr_in6*>(&caddr);
    inet_ntop(AF_INET6, &(in6->sin6_addr), astring.data(), INET6_ADDRSTRLEN);
    std::cout << "client " << astring.data() << " port "
              << ntohs(in6->sin6_port) << " connected.\n";
    // dispatch to threadpool
    // dispatch call packages HandleClient and a ClientCtx into a Task struct
    // the worker thread calls HandleClient(ctx), which casts ctx back to
    // ClientCtx* and does the work
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    auto* ctx = new ClientCtx{.client_fd = client_fd,
                              .home_page = home_page,
                              .files_root = m_files_root,
                              .index = &m_index};
    m_pool.Dispatch({.func = HandleClient, .arg = ctx});
  }
  // only close if signal handler hasn't already closed it
  if (g_listen_fd >= 0) {
    close(listen_fd);
  }
  return EXIT_SUCCESS;
}

}  // namespace searchserver
