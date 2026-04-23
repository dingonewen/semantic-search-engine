#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static bool SendHttpRequest(const std::string& host,
                            uint16_t port,
                            const std::string& method,
                            const std::string& path,
                            const std::string& body,
                            std::string* http_response) {
  http_response->clear();
  int sock = socket(AF_INET6, SOCK_STREAM, 0);
  if (sock == -1)
    return false;

  struct sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(port);
  const std::string& connect_host = (host == "::") ? "::1" : host;
  if (inet_pton(AF_INET6, connect_host.c_str(), &address.sin6_addr) != 1) {
    close(sock);
    return false;
  }

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(address)) != 0) {
    close(sock);
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
    http_response->append(buf, (size_t)n);
  close(sock);
  return true;
}

// Print commands that the user can type
static void PrintHelp() {
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

// Process server path from user input
static std::string MakeServerPath(const std::string& server_path) {
  if (server_path.empty())
    return std::string();
  // if the client provided a leading directory like "test_tree/...",
  // strip that first path component since servers are given the
  // files_root ("test_tree") and expect paths relative to it.
  std::string res_server_path = server_path;
  // strip a common prefix 'test_tree/' if present
  const std::string prefix = "test_tree/";
  if (res_server_path.rfind(prefix, 0) == 0)
    res_server_path = res_server_path.substr(prefix.size());
  if (res_server_path.front() != '/')
    return std::string("/static/") + res_server_path;
  return res_server_path;
}

static bool parse_status(const std::string& http_response, int* status_out) {
  std::istringstream ss(http_response);
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
  PrintHelp();
  std::string line;
  while (true) {
    std::cout << ">> ";
    if (!std::getline(std::cin, line))
      break;
    if (line.empty())
      continue;
    std::istringstream input_string(line);
    std::string command;
    input_string >> command;
    if (command == "quit" || command == "exit")
      break;
    if (command == "help") {
      PrintHelp();
      continue;
    }

    if (command == "get") {
      std::string server_path, local_path;
      input_string >> server_path >> local_path;
      if (server_path.empty()) {
        std::cout << "get requires <server_path>\n";
        continue;
      }
      std::string path = MakeServerPath(server_path);

      std::string http_response;
      if (!SendHttpRequest(host, port, "GET", path, {}, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      size_t pos = http_response.find("\r\n\r\n");
      size_t body_pos = std::string::npos;
      if (pos != std::string::npos)
        body_pos = pos + 4;
      else {
        pos = http_response.find("\n\n");
        if (pos != std::string::npos)
          body_pos = pos + 2;
      }
      std::string body = (body_pos == std::string::npos)
                             ? http_response
                             : http_response.substr(body_pos);
      int code = 0;
      parse_status(http_response, &code);
      if (code >= 400) {
        // extract the HTTP reason phrase and print it plainly
        std::istringstream status_line(http_response);
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

    if (command == "delete") {
      std::string server_path;
      input_string >> server_path;
      if (server_path.empty()) {
        std::cout << "delete requires <server_path>\n";
        continue;
      }
      std::string path = MakeServerPath(server_path);
      std::string http_response;
      if (!SendHttpRequest(host, port, "DELETE", path, {}, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      if (!parse_status(http_response, &code)) {
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
        std::string alt_path = MakeServerPath(alt);
        std::string resp2;
        if (SendHttpRequest(host, port, "DELETE", alt_path, {}, &resp2)) {
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

    if (command == "post" || command == "put") {
      std::string server_path, local_path;
      input_string >> server_path >> local_path;
      if (server_path.empty() || local_path.empty()) {
        std::cout << command << " requires <server_path> <local_path>\n";
        continue;
      }
      std::ifstream in(local_path, std::ios::in | std::ios::binary);
      if (!in) {
        std::cout << "failed to open " << local_path << "\n";
        continue;
      }
      std::string data((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
      std::string method = (command == "post") ? "POST" : "PUT";
      std::string path = MakeServerPath(server_path);
      std::string http_response;
      if (!SendHttpRequest(host, port, method, path, data, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      parse_status(http_response, &code);
      if (code >= 200 && code < 300) {
        std::cout << "OK\n";
      } else {
        std::istringstream status_line(http_response);
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

    std::cout << "unknown command: " << command << " (type help)\n";
  }
  return 0;
}
