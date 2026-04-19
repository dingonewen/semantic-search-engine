#include "HttpServer.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <tuple>
#include <vector>

#include "Request.hpp"
#include "Response.hpp"
#include "StaticFile.hpp"

namespace searchserver {

HttpServer::HttpServer(int port,
                       const std::string& files_root,
                       size_t num_threads)
    : m_port(port), m_files_root(files_root), m_pool(num_threads), m_index() {}

HttpServer::~HttpServer() {}

static std::string read_file_raw(const std::string& path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in)
    return {};
  std::string s((std::istreambuf_iterator<char>(in)),
                std::istreambuf_iterator<char>());
  return s;
}

int HttpServer::run(const std::string& initial_response_path) {
  std::string initial_response = read_file_raw(initial_response_path);
  std::string initial_html = initial_response;
  if (!initial_response.empty()) {
    // accept both CRLF-CRLF and LF-LF separators (some files use only LF)
    size_t sep = initial_response.find("\r\n\r\n");
    if (sep != std::string::npos) {
      initial_html = initial_response.substr(sep + 4);
    } else {
      sep = initial_response.find("\n\n");
      if (sep != std::string::npos)
        initial_html = initial_response.substr(sep + 2);
    }
  }

  // build index if files_root provided
  if (!m_files_root.empty()) {
    m_index.build(m_files_root);
  }

  int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("socket");
    return 1;
  }
  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(m_port));
  if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(listenfd);
    return 1;
  }
  if (listen(listenfd, 16) < 0) {
    perror("listen");
    close(listenfd);
    return 1;
  }

  std::cout << "accepting connections...\n";

  while (true) {
    struct sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    int client = accept(listenfd, (struct sockaddr*)&peer, &plen);
    if (client < 0) {
      if (errno == EINTR)
        continue;
      perror("accept");
      continue;
    }

    // print client address
    char peer_str[INET_ADDRSTRLEN] = "unknown";
    inet_ntop(AF_INET, &peer.sin_addr, peer_str, sizeof(peer_str));
    std::cout << "client " << peer_str << " connected.\n";

    // dispatch client handling to threadpool
    m_pool.dispatch(
        {[](void* arg) {
           auto* ctx = static_cast<
               std::tuple<int, std::string, std::string, InvertedIndex*>*>(arg);
           int client = std::get<0>(*ctx);
           std::string initial_html = std::get<1>(*ctx);
           std::string files_root = std::get<2>(*ctx);
           InvertedIndex* index = std::get<3>(*ctx);

           // read request headers (and possibly some of the body)
           std::string req;
           char buf[4096];
           ssize_t n = 0;
           while ((n = recv(client, buf, sizeof(buf), 0)) > 0) {
             req.append(buf, static_cast<size_t>(n));
             if (req.find("\r\n\r\n") != std::string::npos)
               break;
             if (req.size() > 65536)
               break;
           }

           // split headers and any pre-read body
           size_t hdrsep = req.find("\r\n\r\n");
           size_t hdrlen = std::string::npos;
           if (hdrsep != std::string::npos)
             hdrlen = hdrsep + 4;
           else {
             hdrsep = req.find("\n\n");
             if (hdrsep != std::string::npos)
               hdrlen = hdrsep + 2;
           }
           std::string header_part =
               (hdrlen == std::string::npos) ? req : req.substr(0, hdrlen);
           std::string body_part = (hdrlen == std::string::npos)
                                       ? std::string()
                                       : req.substr(hdrlen);

           Request r = parse_request(header_part);
           // defensive normalization: trim trailing CR/LF/whitespace from
           // method and path
           auto trim_trailing = [](std::string s) -> std::string {
             while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                                   isspace((unsigned char)s.back())))
               s.pop_back();
             return s;
           };
           r.method = trim_trailing(r.method);
           r.path = trim_trailing(r.path);

           // write debug info to log file
           std::ostringstream dbg;
           dbg << "DEBUG: header_part:\n"
               << header_part << "\n---END HEADER---\n";
           dbg << "DEBUG: parsed method='" << r.method << "' path='" << r.path
               << "' query='" << r.query << "'\n";
           try {
             std::ofstream flog("/tmp/searchserver.log", std::ios::app);
             if (flog)
               flog << dbg.str();
           } catch (...) {
           }

           std::string out;
           if (r.method == "GET") {
             if (r.path == "/" || r.path.empty()) {
               out = make_response(200, initial_html, "text/html");
             } else if (r.path == "/query") {
               auto terms = split_terms(r.query);
               auto results = index->search_and_rank(terms);
               std::string body = initial_html;
               std::ostringstream ins;
               ins << "<p><br>" << results.size() << " results found<p><ul>\n";
               for (auto& p : results) {
                 std::string display = p.first;
                 ins << " <li> <a href=\"/static/" << display << "\">"
                     << display << "</a> [" << p.second << "]<br>\n";
               }
               ins << "</ul>\n";
               auto pos = body.rfind("</body>");
               if (pos != std::string::npos)
                 body.insert(pos, ins.str());
               else
                 body += ins.str();
               out = make_response(200, body, "text/html");
             } else if (r.path.rfind("/static/", 0) == 0) {
               std::string rel = r.path.substr(8);
               auto resp = serve_static(files_root, rel);
               out = resp;
             } else {
               out = make_response(404, "<h1>404 Not Found</h1>", "text/html");
             }
           } else {
             // For non-GET methods, support PUT/POST/DELETE on /static/
             if (r.path.rfind("/static/", 0) == 0) {
               std::string rel = r.path.substr(8);
               // determine content-length
               size_t content_len = 0;
               auto it = r.headers.find("Content-Length");
               if (it != r.headers.end()) {
                 try {
                   content_len = static_cast<size_t>(std::stoul(it->second));
                 } catch (...) {
                   content_len = 0;
                 }
               }
               // read remaining body if any
               while (body_part.size() < content_len) {
                 ssize_t m = recv(client, buf, sizeof(buf), 0);
                 if (m <= 0)
                   break;
                 body_part.append(buf, static_cast<size_t>(m));
               }
               if (r.method == "PUT") {
                 out = static_put(files_root, rel, body_part, true);
               } else if (r.method == "POST") {
                 out = static_put(files_root, rel, body_part, false);
               } else if (r.method == "DELETE") {
                 out = static_delete(files_root, rel);
               } else {
                 out = make_response(501, "<h1>501 Not Implemented</h1>",
                                     "text/html", "Not Implemented");
               }
             } else {
               out = make_response(501, "<h1>501 Not Implemented</h1>",
                                   "text/html", "Not Implemented");
             }
           }

           // send
           ssize_t to_send = static_cast<ssize_t>(out.size());
           const char* ptr = out.data();
           while (to_send > 0) {
             ssize_t w = send(client, ptr, static_cast<size_t>(to_send), 0);
             if (w <= 0)
               break;
             to_send -= w;
             ptr += w;
           }
           shutdown(client, SHUT_RDWR);
           close(client);
           delete ctx;
         },
         new std::tuple<int, std::string, std::string, InvertedIndex*>(
             client, initial_html, m_files_root, &m_index)});
  }

  close(listenfd);
  return 0;
}

}  // namespace searchserver
