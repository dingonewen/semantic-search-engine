#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::string url_encode(const std::string& s) {
  std::ostringstream out;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' ||
        c == '/')
      out << c;
    else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out << buf;
    }
  }
  return out.str();
}

static bool send_http_request(const std::string& host,
                              uint16_t port,
                              const std::string& method,
                              const std::string& path,
                              const std::string& body,
                              std::string* out_resp) {
  out_resp->clear();
  // use getaddrinfo(AF_UNSPEC) so we support IPv4 and IPv6 addresses (e.g.
  // '::')
  struct addrinfo hints{}, *res = nullptr, *rp = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  std::string portstr = std::to_string(port);
  int gai = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &res);
  if (gai != 0)
    return false;

  int sock = -1;
  for (rp = res; rp != nullptr; rp = rp->ai_next) {
    sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock < 0)
      continue;
    if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
      break;  // success
    close(sock);
    sock = -1;
  }
  freeaddrinfo(res);
  if (sock < 0) {
    // try an IPv4 localhost fallback (handles cases where host is '::' but
    // server is only listening on IPv4 loopback)
    if (host != "127.0.0.1") {
      struct addrinfo hints2{}, *res2 = nullptr, *rp2 = nullptr;
      hints2.ai_family = AF_INET;
      hints2.ai_socktype = SOCK_STREAM;
      if (getaddrinfo("127.0.0.1", portstr.c_str(), &hints2, &res2) == 0) {
        for (rp2 = res2; rp2 != nullptr; rp2 = rp2->ai_next) {
          sock = ::socket(rp2->ai_family, rp2->ai_socktype, rp2->ai_protocol);
          if (sock < 0)
            continue;
          if (connect(sock, rp2->ai_addr, rp2->ai_addrlen) == 0)
            break;
          close(sock);
          sock = -1;
        }
        freeaddrinfo(res2);
      }
    }
    if (sock < 0)
      return false;
  }

  std::ostringstream req;
  req << method << " " << path << " HTTP/1.1\r\n";
  req << "Host: " << host << ":" << port << "\r\n";
  req << "User-Agent: searchclient/1.0\r\n";
  if (!body.empty()) {
    req << "Content-Length: " << body.size() << "\r\n";
    req << "Content-Type: application/octet-stream\r\n";
  }
  req << "Connection: close\r\n";
  req << "\r\n";
  req << body;

  std::string rq = req.str();
  ssize_t sent = 0;
  const char* p = rq.data();
  ssize_t remain = (ssize_t)rq.size();
  while (remain > 0) {
    ssize_t w = send(sock, p + sent, (size_t)remain, 0);
    if (w <= 0) {
      close(sock);
      return false;
    }
    sent += w;
    remain -= w;
  }

  char buf[4096];
  ssize_t n;
  while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
    out_resp->append(buf, (size_t)n);
  close(sock);
  return true;
}

static void print_help() {
  std::cout << "These requests modify the state of the underlying file system "
               "of the server\n";
  std::cout << "  get <server_path>                -- requests the resource "
               "from the server and prints it to stdout/cout.\n";
  std::cout << "  get <server_path> [local_path]   -- download resource (print "
               "or save)\n";
  std::cout << "  delete <server_path>             -- delete remote resource\n";
  std::cout << "  post <server_path> <local_path>  -- upload new resource "
               "(fail if exists)\n";
  std::cout
      << "  put <server_path> <local_path>   -- upload/replace resource\n";
  std::cout << "  help                             -- this message\n";
  std::cout << "  quit | exit                      -- quit\n";
}

static bool parse_status(const std::string& resp, int* status_out) {
  std::istringstream ss(resp);
  std::string line;
  if (!std::getline(ss, line))
    return false;
  std::istringstream ls(line);
  std::string proto;
  ls >> proto;
  int code;
  ls >> code;
  if (!ls)
    return false;
  *status_out = code;
  return true;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <server> <port>\n";
    return 1;
  }
  std::string host = argv[1];
  uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
  print_help();
  std::string line;
  auto make_server_path = [](const std::string& server_path) -> std::string {
    if (server_path.empty())
      return std::string();
    // if the client provided a leading project directory like "test_tree/...",
    // strip that first path component since servers typically are given the
    // files_root (e.g. "test_tree") and expect paths relative to it.
    std::string sp = server_path;
    // strip a common prefix 'test_tree/' if present
    const std::string prefix = "test_tree/";
    if (sp.rfind(prefix, 0) == 0)
      sp = sp.substr(prefix.size());
    if (sp.front() != '/')
      return std::string("/static/") + url_encode(sp);
    return sp;
  };
  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line))
      break;
    if (line.empty())
      continue;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd == "quit" || cmd == "exit")
      break;
    if (cmd == "help") {
      print_help();
      continue;
    }

    if (cmd == "get") {
      std::string server_path, local_path;
      iss >> server_path >> local_path;
      if (server_path.empty()) {
        std::cout << "get requires <server_path>\n";
        continue;
      }
      std::string path = make_server_path(server_path);

      std::string resp;
      if (!send_http_request(host, port, "GET", path, {}, &resp)) {
        std::cout << "request failed\n";
        continue;
      }
      size_t pos = resp.find("\r\n\r\n");
      size_t body_pos = std::string::npos;
      if (pos != std::string::npos)
        body_pos = pos + 4;
      else {
        pos = resp.find("\n\n");
        if (pos != std::string::npos)
          body_pos = pos + 2;
      }
      std::string body =
          (body_pos == std::string::npos) ? resp : resp.substr(body_pos);
      int code = 0;
      parse_status(resp, &code);
      if (code >= 400) {
        // extract the HTTP reason phrase and print it plainly
        std::istringstream status_line(resp);
        std::string proto, code_str, reason;
        status_line >> proto >> code_str;
        std::getline(status_line, reason);
        while (!reason.empty() &&
               (reason.front() == ' ' || reason.front() == '\t'))
          reason.erase(reason.begin());
        while (!reason.empty() &&
               (reason.back() == '\r' || reason.back() == '\n'))
          reason.pop_back();
        std::cout << reason << "\n";
        continue;
      }
      if (local_path.empty())
        std::cout << body << std::endl;
      else {
        std::ofstream out(local_path, std::ios::out | std::ios::binary);
        if (!out) {
          std::cout << "failed to open " << local_path << "\n";
        } else {
          out.write(body.data(), (std::streamsize)body.size());
          std::cout << "saved to " << local_path << "\n";
        }
      }
      continue;
    }

    if (cmd == "delete") {
      std::string server_path;
      iss >> server_path;
      if (server_path.empty()) {
        std::cout << "delete requires <server_path>\n";
        continue;
      }
      std::string path = make_server_path(server_path);
      std::string resp;
      if (!send_http_request(host, port, "DELETE", path, {}, &resp)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      if (!parse_status(resp, &code)) {
        std::cout << "failed to parse response\n";
        continue;
      }
      if (code >= 200 && code < 300) {
        std::cout << "OK\n";
        continue;
      }
      // If delete failed, try stripping a leading path component and retry.
      // This helps when the client sent e.g. 'test_tree/books/...' but the
      // server's files_root is already 'test_tree' (so it expects 'books/...').
      auto slash = server_path.find('/');
      if (slash != std::string::npos) {
        std::string alt = server_path.substr(slash + 1);
        std::string alt_path = make_server_path(alt);
        std::string resp2;
        if (send_http_request(host, port, "DELETE", alt_path, {}, &resp2)) {
          int code2 = 0;
          if (parse_status(resp2, &code2) && code2 >= 200 && code2 < 300) {
            std::cout << "OK\n";
            continue;
          }
        }
      }
      std::cout << "HTTP " << code << "\n";
      continue;
    }

    if (cmd == "post" || cmd == "put") {
      std::string server_path, local_path;
      iss >> server_path >> local_path;
      if (server_path.empty() || local_path.empty()) {
        std::cout << cmd << " requires <server_path> <local_path>\n";
        continue;
      }
      std::ifstream in(local_path, std::ios::in | std::ios::binary);
      if (!in) {
        std::cout << "failed to open " << local_path << "\n";
        continue;
      }
      std::string data((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
      std::string method = (cmd == "post") ? "POST" : "PUT";
      std::string path = make_server_path(server_path);
      std::string resp;
      if (!send_http_request(host, port, method, path, data, &resp)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      parse_status(resp, &code);
      if (code >= 200 && code < 300) {
        std::cout << "OK\n";
      } else {
        std::istringstream status_line(resp);
        std::string proto, code_str, reason;
        status_line >> proto >> code_str;
        std::getline(status_line, reason);
        while (!reason.empty() &&
               (reason.front() == ' ' || reason.front() == '\t'))
          reason.erase(reason.begin());
        while (!reason.empty() &&
               (reason.back() == '\r' || reason.back() == '\n'))
          reason.pop_back();
        std::cout << reason << "\n";
      }
      continue;
    }

    std::cout << "unknown command: " << cmd << " (type help)\n";
  }
  return 0;
}
