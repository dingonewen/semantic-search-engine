#include "./HttpServer.hpp"
#include "./HttpRequest.hpp"   // ParseRequest, SplitTerms
#include "./HttpResponse.hpp"  // MakeResponse, k_http_*
#include "./InvertedIndex.hpp"
#include "./StaticFile.hpp"    // StaticGet, StaticPut, StaticDelete
#include "./VectorClient.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>  // NOLINT(misc-include-cleaner) — provides timeval
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
std::atomic<int> g_done{0};  // set to 1 on SIGINT
std::atomic<int> g_listen_fd{
    -1};  // set before accept loop so handler can close it

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
  InvertedIndex* index;          // pointer to the search index for queries
  std::shared_mutex* index_mtx;  // guards index for concurrent read/write
  VectorClient* vec;             // embedding service client (may return empty)
};

// Reciprocal Rank Fusion: merges BM25 and semantic rankings into one list.
// score(d) = 1/(k+rank_bm25) + 1/(k+rank_semantic), with k=60 (standard).
auto RrfMerge(const std::vector<std::pair<std::string, int>>& bm25,
              const std::vector<std::pair<std::string, float>>& semantic)
    -> std::vector<std::pair<std::string, float>> {
  constexpr float k_rrf_k = 60.0f;
  std::unordered_map<std::string, float> scores;
  for (size_t i = 0; i < bm25.size(); ++i) {
    scores[bm25[i].first] +=
        1.0f / (k_rrf_k + static_cast<float>(i) + 1.0f);
  }
  for (size_t i = 0; i < semantic.size(); ++i) {
    scores[semantic[i].first] +=
        1.0f / (k_rrf_k + static_cast<float>(i) + 1.0f);
  }
  std::vector<std::pair<std::string, float>> merged(scores.begin(),
                                                     scores.end());
  std::ranges::sort(merged,
                    [](const auto& a, const auto& b) { return a.second > b.second; });
  return merged;
}

/*
HandleClient's inner logic was split into ReadHeaders, ReadBody,
ReadRawRequest, HandleGetRequest, HandleStaticMutation, and SendAll for
tidy-check (cognitive complexity limit)
*/
// reads raw bytes from fd into raw until \r\n\r\n is found
auto ReadHeaders(int fd, std::string* raw) -> bool {
  std::array<char, 4096> buf{};
  while (raw->find("\r\n\r\n") == std::string::npos) {
    const ssize_t n = read(fd, buf.data(), buf.size() - 1);
    if (n < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        if (g_done != 0) {
          return false;  // server shutting down
        }
        continue;
      }
      return false;
    }
    if (n == 0) {
      return false;  // client disconnected
    }
    raw->append(buf.data(), static_cast<size_t>(n));
  }
  return true;
}

// reads body bytes based on Content-Length header, appending to raw
void ReadBody(int fd, std::string* raw, size_t body_offset) {
  constexpr std::string_view k_cl_key = "content-length:";
  std::string lower_hdr = raw->substr(0, body_offset);
  for (char& c : lower_hdr) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  const size_t cl_pos = lower_hdr.find(k_cl_key);
  if (cl_pos == std::string::npos) {
    return;
  }
  size_t val_pos = cl_pos + k_cl_key.size();
  while (val_pos < lower_hdr.size() && lower_hdr[val_pos] == ' ') {
    ++val_pos;
  }
  if (val_pos >= raw->size() ||
      std::isdigit(static_cast<unsigned char>((*raw)[val_pos])) == 0) {
    return;
  }
  const size_t content_length = std::stoul(raw->substr(val_pos));
  std::array<char, 4096> buf{};
  size_t body_read = raw->size() - body_offset;
  while (body_read < content_length) {
    const size_t to_read = std::min(content_length - body_read, buf.size() - 1);
    const ssize_t n = read(fd, buf.data(), to_read);
    if (n < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        if (g_done != 0) {
          break;
        }
        continue;
      }
      break;
    }
    if (n == 0) {
      break;
    }
    raw->append(buf.data(), static_cast<size_t>(n));
    body_read += static_cast<size_t>(n);
  }
}

// read until \r\n\r\n (end of headers), chunk by chunk, then read body
auto ReadRawRequest(int fd) -> std::string {
  std::string raw;
  if (!ReadHeaders(fd, &raw)) {
    return raw;
  }
  const size_t body_offset = raw.find("\r\n\r\n") + 4;
  ReadBody(fd, &raw, body_offset);
  return raw;
}

// Build a space-joined query string from term list.
auto JoinTerms(const std::vector<std::string>& terms) -> std::string {
  std::string out;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (i > 0) {
      out += ' ';
    }
    out += terms[i];
  }
  return out;
}

// Escape a string for embedding in a JSON string literal.
auto JsonStr(const std::string& s) -> std::string {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// Serialize a ranked list as a JSON array: [{"id":"...","score":N},...]
template <typename T>
auto RankedToJson(const std::vector<std::pair<std::string, T>>& v)
    -> std::string {
  std::string out = "[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i > 0) {
      out += ',';
    }
    std::ostringstream ss;
    ss << v[i].second;
    out += "{\"id\":\"" + JsonStr(v[i].first) + "\",\"score\":" + ss.str() +
           "}";
  }
  return out + "]";
}

// Escape special HTML characters for safe embedding in HTML.
auto HtmlEscape(const std::string& s) -> std::string {
  std::string out;
  out.reserve(s.size());
  for (const char c : s) {
    switch (c) {
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// Format float to two decimal places as a string.
auto Fmt2f(float v) -> std::string {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(4) << v;
  return oss.str();
}

// handles GET requests: route to home page, hybrid query results, or static file
auto HandleGetRequest(const Request& r, ClientCtx* ctx) -> std::string {
  if (r.path == "/" || r.path.empty()) {
    return MakeResponse(k_http_ok, ctx->home_page, "text/html");
  }
  if (r.path == "/query") {
    auto terms = SplitTerms(r.query);
    const std::string query_str = JoinTerms(terms);

    // BM25 (inverted index)
    std::vector<std::pair<std::string, int>> bm25;
    {
      const std::shared_lock<std::shared_mutex> lock(*ctx->index_mtx);
      bm25 = ctx->index->SearchAndRank(terms);
    }

    // Semantic (embedding service — empty if service is down)
    const auto semantic = ctx->vec->Search(query_str);

    // Hybrid merge via Reciprocal Rank Fusion
    const auto results = RrfMerge(bm25, semantic);

    std::string links;
    for (const auto& p : results) {
      links += "<li><a href=\"/static/" + p.first + "\">" + p.first +
               "</a> <span style=\"color:#888\">[hybrid: " + Fmt2f(p.second) +
               "]</span></li>\n";
    }
    std::string body = ctx->home_page;
    body += "<p>" + std::to_string(results.size()) +
            " results found for <b>" + query_str + "</b>";
    if (!semantic.empty()) {
      body += " <span style=\"color:green;font-size:80%\">(semantic active)</span>";
    }
    body += "</p>\n<ul>\n" + links + "</ul>\n";
    return MakeResponse(k_http_ok, body, "text/html");
  }
  if (r.path == "/api/search") {
    const auto terms = SplitTerms(r.query);
    if (terms.empty()) {
      return MakeResponse(k_http_ok, "{\"error\":\"empty query\"}",
                          "application/json");
    }
    const std::string query_str = JoinTerms(terms);

    std::vector<std::pair<std::string, int>> bm25;
    {
      const std::shared_lock<std::shared_mutex> lock(*ctx->index_mtx);
      bm25 = ctx->index->SearchAndRank(terms);
    }
    const auto semantic = ctx->vec->Search(query_str);
    const auto hybrid = RrfMerge(bm25, semantic);

    const std::string json =
        "{\"query\":\"" + JsonStr(query_str) + "\"," +
        "\"bm25\":" + RankedToJson(bm25) + "," +
        "\"semantic\":" + RankedToJson(semantic) + "," +
        "\"hybrid\":" + RankedToJson(hybrid) + "}";
    return MakeResponse(k_http_ok, json, "application/json");
  }
  if (r.path == "/ask") {
    const auto terms = SplitTerms(r.query);
    if (terms.empty()) {
      return MakeResponse(k_http_ok, ctx->home_page, "text/html");
    }
    const std::string question = JoinTerms(terms);
    auto [answer, sources] = ctx->vec->Ask(question);

    std::string body = ctx->home_page;
    body +=
        "<div style=\"max-width:700px;margin:20px auto;background:#f8f8f8;"
        "border:1px solid #ddd;border-radius:8px;padding:20px;\">\n"
        "<h3 style=\"margin-top:0\">AI Answer</h3>\n";
    if (answer.empty()) {
      body +=
          "<p style=\"color:#888\">Embed service unavailable or no documents "
          "indexed. Run <code>./start.sh</code> first.</p>\n";
    } else {
      body += "<div style=\"white-space:pre-wrap;line-height:1.5\">" +
              HtmlEscape(answer) + "</div>\n";
      if (!sources.empty()) {
        body +=
            "<hr style=\"margin:16px 0\">"
            "<p style=\"color:#666;font-size:85%;margin:0 0 8px\">Sources:</p>"
            "\n<ul>\n";
        for (const auto& src : sources) {
          body += "<li><a href=\"/static/" + src + "\">" + HtmlEscape(src) +
                  "</a></li>\n";
        }
        body += "</ul>\n";
      }
    }
    body += "<p><a href=\"/\">&larr; Back</a></p>\n</div>\n";
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
    const auto resp =
        StaticPut(ctx->files_root, rel, r.body, true);  // PUT has body
    if (resp.find("HTTP/1.1 2") != std::string::npos) {
      {
        const std::unique_lock<std::shared_mutex> lock(*ctx->index_mtx);
        ctx->index->AddFile(rel);
      }
      ctx->vec->AddDoc(rel, r.body);  // update vector index
    }
    return resp;
  }
  if (r.method == "POST") {
    const auto resp =
        StaticPut(ctx->files_root, rel, r.body, false);  // POST has body
    if (resp.find("HTTP/1.1 2") != std::string::npos) {
      {
        const std::unique_lock<std::shared_mutex> lock(*ctx->index_mtx);
        ctx->index->AddFile(rel);
      }
      ctx->vec->AddDoc(rel, r.body);  // update vector index
    }
    return resp;
  }
  if (r.method == "DELETE") {
    const auto resp = StaticDelete(ctx->files_root, rel);
    if (resp.find("HTTP/1.1 2") != std::string::npos) {
      {
        const std::unique_lock<std::shared_mutex> lock(*ctx->index_mtx);
        ctx->index->RemoveFile(rel);
      }
      ctx->vec->RemoveDoc(rel);  // update vector index
    }
    return resp;
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
  // 1-second read timeout: lets the thread wake up and check g_done on Ctrl+C
  // instead of blocking in read() forever on a keep-alive connection
  // NOLINTNEXTLINE(misc-include-cleaner)
  struct timeval tv{.tv_sec = 1, .tv_usec = 0};
  // NOLINTNEXTLINE(misc-include-cleaner)
  setsockopt(ctx->client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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
      m_index(),
      m_vec(),
      m_pool(num_threads) {
  m_index.Build(m_files_root);
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

  struct sigaction sigact{};          // NOLINT(misc-include-cleaner)
  sigact.sa_handler = SigintHandler;  // NOLINT(misc-include-cleaner)
  // remove SA_RESTART flag to continue re-check g_done
  sigact.sa_flags = 0;
  sigaction(SIGINT, &sigact, nullptr);  // NOLINT(misc-include-cleaner)

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
  // NOLINTNEXTLINE(misc-include-cleaner)
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
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
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
                              .index = &m_index,
                              .index_mtx = &m_index_mtx,
                              .vec = &m_vec};
    m_pool.Dispatch({.func = HandleClient, .arg = ctx});
  }
  // only close if signal handler hasn't already closed it
  if (g_listen_fd >= 0) {
    close(listen_fd);
  }
  return EXIT_SUCCESS;
}

}  // namespace searchserver
