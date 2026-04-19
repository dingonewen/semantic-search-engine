#include "client_shell.hpp"

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

static bool send_http_get(const std::string& host,
                          uint16_t port,
                          const std::string& path,
                          std::string* out_resp) {
  out_resp->clear();
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return false;

  struct sockaddr_in srv{};
  srv.sin_family = AF_INET;
  srv.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &srv.sin_addr) <= 0) {
    // try resolving via getaddrinfo
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
      struct sockaddr_in* a = (struct sockaddr_in*)res->ai_addr;
      srv.sin_addr = a->sin_addr;
      freeaddrinfo(res);
    } else {
      close(sock);
      return false;
    }
  }

  if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) != 0) {
    close(sock);
    return false;
  }

  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\n";
  req << "Host: " << host << ":" << port << "\r\n";
  req << "Connection: close\r\n";
  req << "User-Agent: client-shell/1.0\r\n";
  req << "\r\n";

  std::string rq = req.str();
  ssize_t sent = 0;
  const char* p = rq.data();
  ssize_t remain = (ssize_t)rq.size();
  while (remain > 0) {
    ssize_t w = ::send(sock, p + sent, (size_t)remain, 0);
    if (w <= 0) {
      close(sock);
      return false;
    }
    sent += w;
    remain -= w;
  }

  char buf[4096];
  ssize_t n;
  while ((n = ::recv(sock, buf, sizeof(buf), 0)) > 0) {
    out_resp->append(buf, (size_t)n);
  }

  close(sock);
  return true;
}

static void print_help() {
  std::cout << "These requests modify the state of the underlying file system "
               "of the server\n";
  std::cout << "  home                 -- GET /\n";
  std::cout << "  query <terms>        -- GET /query?terms=<terms> (space "
               "separated)\n";
  std::cout << "  get <server_path> - requests the resource from the server "
               "and prints it to stdout/cout.\n";
  std::cout << "  fetch <path>         -- GET <path> (any path)\n";
  std::cout << "  raw                  -- toggle printing raw response headers "
               "(default: show body)\n";
  std::cout << "  quit | exit          -- quit\n";
}

int run_client_shell(const ClientConfig& cfg) {
  bool show_raw = false;
  std::string line;
  print_help();
  for (;;) {
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
    if (cmd == "raw") {
      show_raw = !show_raw;
      std::cout << "raw=" << (show_raw ? "ON" : "OFF") << "\n";
      continue;
    }

    std::string path;
    std::string saved_local;
    if (cmd == "home")
      path = "/";
    else if (cmd == "query") {
      std::string rest;
      std::getline(iss, rest);
      if (!rest.empty() && rest.front() == ' ')
        rest.erase(rest.begin());
      std::string enc = url_encode(rest);
      path = std::string("/query?terms=") + enc;
    } else if (cmd == "get") {
      // support: get <server_path> [local_path]
      std::string server_path, local_path;
      iss >> server_path;  // first token is the path on server
      iss >> local_path;   // optional local filename to save
      if (server_path.empty()) {
        std::cout << "get requires <server_path>\n";
        continue;
      }
      std::string enc = url_encode(server_path);
      if (enc.empty() || enc.front() != '/')
        path = std::string("/static/") + enc;
      else
        path = std::string("/static") + enc;
      saved_local = local_path;
    } else if (cmd == "fetch") {
      std::string rest;
      std::getline(iss, rest);
      if (!rest.empty() && rest.front() == ' ')
        rest.erase(rest.begin());
      if (rest.empty()) {
        std::cout << "fetch requires a path\n";
        continue;
      }
      path = rest;
    } else {
      std::cout << "unknown command: " << cmd << " (type 'help')\n";
      continue;
    }

    std::string resp;
    bool ok = send_http_get(cfg.host, cfg.port, path, &resp);
    if (!ok) {
      std::cout << "request failed\n";
      continue;
    }
    // split headers/body
    auto pos = resp.find("\r\n\r\n");
    size_t body_pos = std::string::npos;
    if (pos != std::string::npos)
      body_pos = pos + 4;
    else {
      pos = resp.find("\n\n");
      if (pos != std::string::npos)
        body_pos = pos + 2;
    }
    if (show_raw || body_pos == std::string::npos) {
      std::cout << resp << "\n";
    } else {
      std::string body = resp.substr(body_pos);
      // if a local path was provided to 'get', save; otherwise print to stdout
      if (!saved_local.empty()) {
        std::ofstream out(saved_local, std::ios::out | std::ios::binary);
        if (!out) {
          std::cout << "failed to open " << saved_local << "\n";
        } else {
          out.write(body.data(), (std::streamsize)body.size());
          std::cout << "saved to " << saved_local << "\n";
        }
      } else {
        std::cout << body << "\n";
      }
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  ClientConfig cfg;
  if (argc >= 2)
    cfg.host = argv[1];
  if (argc >= 3)
    cfg.port = static_cast<uint16_t>(std::stoi(argv[2]));
  return run_client_shell(cfg);
}
