#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Performs a complete http request/response cycle over a TCP socket
// host: server's IPv6 address string
// port: the server port number
// path: the request path
// body: the request body, used for POST/PUT. For GET/DELETE, the caller passes
// {}
// http_response: output parameter, store raw http response from the server
static bool SendHttpRequest(const std::string& host,
                            uint16_t port,
                            const std::string& method,
                            const std::string& path,
                            const std::string& body,
                            std::string* http_response) {
  http_response->clear();
  int sock = socket(AF_INET6, SOCK_STREAM, 0);
  if (sock == -1) {
    std::cerr << "socket() failed: " << std::strerror(errno) << "\n";
    return false;
  }

  // Set up the server address
  struct sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(port);
  const std::string& connect_host = (host == "::") ? "::1" : host;
  int pton_rv = inet_pton(AF_INET6, connect_host.c_str(), &address.sin6_addr);
  if (pton_rv != 1) {
    if (pton_rv == 0) {
      std::cerr << "inet_pton: '" << connect_host
                << "' is not a valid IPv6 address\n";
    } else {
      std::cerr << "inet_pton failed for '" << connect_host
                << "': " << std::strerror(errno) << "\n";
    }
    close(sock);
    return false;
  }

  // Connect to server
  if (connect(sock, reinterpret_cast<struct sockaddr*>(&address),
              sizeof(address)) != 0) {
    std::cerr << "connect to " << connect_host << ":" << port
              << " failed: " << std::strerror(errno) << "\n";
    close(sock);
    return false;
  }

  // Send request
  std::ostringstream request;
  request << method << " " << path << " HTTP/1.1\r\n";
  request << "Host: " << host << ":" << port << "\r\n";
  request << "User-Agent: searchclient/1.0\r\n";
  if (!body.empty()) {
    request << "Content-Length: " << body.size() << "\r\n";
    request << "Content-Type: application/octet-stream\r\n";
  }
  request << "Connection: close\r\n";
  request << "\r\n";
  request << body;

  std::string req = request.str();
  ssize_t sent = 0;
  ssize_t remain = (ssize_t)req.size();
  while (remain > 0) {
    ssize_t res = send(sock, req.data() + sent, (size_t)remain, 0);
    if (res <= 0) {
      std::cerr << "send() failed: " << std::strerror(errno) << "\n";
      close(sock);
      return false;
    }
    sent += res;
    remain -= res;
  }

  // Receive data from server socket
  char buf[4096];
  ssize_t n;
  while ((n = recv(sock, buf, sizeof(buf), 0)) > 0)
    http_response->append(buf, (size_t)n);
  if (n < 0) {
    std::cerr << "recv() failed: " << std::strerror(errno) << "\n";
    close(sock);
    return false;
  }
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

// Extracts the http status code from http response and writes it into
// status_out. Returns true on success and false if parsing fails
static bool ParseStatus(const std::string& http_response, int* status_out) {
  std::istringstream ss(http_response);
  std::string line;
  if (!std::getline(ss, line))
    return false;
  std::istringstream ls(line);
  std::string proto;
  ls >> proto;
  int code;
  ls >> code;
  // Check if the stream is in fail state (>> code didn't successfully parse an
  // int)
  if (!ls)
    return false;
  *status_out = code;
  return true;
}

/*
 * Accept and process Command Line Shell
 * This program connects to the server and begins reading input from the command
 * line. Read lines of input until the user types CTRL + D (eof).
 */
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
    // Ctrl + D signals eof on stdin and std::getline returns a falsy stream ref
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
      std::string path = "/static/" + server_path;

      std::string http_response;
      if (!SendHttpRequest(host, port, "GET", path, {}, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      // Locates the body of the http response
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
      ParseStatus(http_response, &code);
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
      // Print the response body to stdout
      if (local_path.empty())
        std::cout << body << std::endl;
      else {  // Print the reponse body to local file
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
      std::string path = "/static/" + server_path;
      std::string http_response;
      if (!SendHttpRequest(host, port, "DELETE", path, {}, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      if (!ParseStatus(http_response, &code)) {
        std::cout << "failed to parse response\n";
        continue;
      }
      if (code >= 200 && code < 300) {
        std::cout << "OK\n";
        continue;
      }
      std::cout << "HTTP " << code << "\n";
      continue;
    }

    // Send data to server
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
      std::string path = "/static/" + server_path;
      std::string http_response;
      if (!SendHttpRequest(host, port, method, path, data, &http_response)) {
        std::cout << "request failed\n";
        continue;
      }
      int code = 0;
      ParseStatus(http_response, &code);
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
