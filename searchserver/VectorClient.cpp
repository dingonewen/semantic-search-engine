#include "VectorClient.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace searchserver {

namespace {

constexpr int k_connect_timeout_sec = 1;
constexpr size_t k_text_limit = 2048;

// Sends all bytes in buf to fd, returning false on write error.
auto SendAll(int fd, const std::string& buf) -> bool {
  auto remaining = static_cast<ssize_t>(buf.size());
  const char* ptr = buf.data();
  while (remaining > 0) {
    const ssize_t w = write(fd, ptr, static_cast<size_t>(remaining));
    if (w <= 0) {
      return false;
    }
    ptr += w;
    remaining -= w;
  }
  return true;
}

// Reads bytes from fd until the server closes the connection (Connection:
// close). Returns the full raw HTTP response.
auto RecvAll(int fd) -> std::string {
  std::string raw;
  std::array<char, 4096> buf{};
  while (true) {
    const ssize_t n = read(fd, buf.data(), buf.size() - 1);
    if (n <= 0) {
      break;
    }
    raw.append(buf.data(), static_cast<size_t>(n));
  }
  return raw;
}

}  // namespace

VectorClient::VectorClient(int port) : m_port(port) {}

auto VectorClient::Connect() const -> int {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  // Short connect timeout so a missing service fails fast.
  struct timeval tv{.tv_sec = k_connect_timeout_sec, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,  // NOLINT(misc-include-cleaner)
             &tv, sizeof(tv));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(m_port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) !=
      0) {
    close(fd);
    return -1;
  }
  return fd;
}

// static
auto VectorClient::ReadResponse(int fd) -> std::string {
  const std::string raw = RecvAll(fd);
  // Find header/body separator
  const auto sep = raw.find("\r\n\r\n");
  if (sep == std::string::npos) {
    return {};
  }
  return raw.substr(sep + 4);
}

// static
auto VectorClient::JsonEscape(const std::string& s) -> std::string {
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

// static
auto VectorClient::ParseTsv(const std::string& body)
    -> std::vector<std::pair<std::string, float>> {
  std::vector<std::pair<std::string, float>> results;
  std::istringstream iss(body);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) {
      continue;
    }
    const auto tab = line.find('\t');
    if (tab == std::string::npos) {
      continue;
    }
    const std::string doc_id = line.substr(0, tab);
    try {
      const float score = std::stof(line.substr(tab + 1));
      results.emplace_back(doc_id, score);
    } catch (...) {
      // malformed line — skip
    }
  }
  return results;
}

auto VectorClient::Post(const std::string& path,
                        const std::string& json_body) const -> std::string {
  const int fd = Connect();
  if (fd < 0) {
    return {};
  }

  const std::string request =
      "POST " + path +
      " HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " +
      std::to_string(json_body.size()) +
      "\r\n"
      "Connection: close\r\n"
      "\r\n" +
      json_body;

  std::string body;
  if (SendAll(fd, request)) {
    body = ReadResponse(fd);
  }
  close(fd);
  return body;
}

auto VectorClient::Delete(const std::string& path) const -> std::string {
  const int fd = Connect();
  if (fd < 0) {
    return {};
  }

  const std::string request =
      "DELETE " + path +
      " HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Connection: close\r\n"
      "\r\n";

  std::string body;
  if (SendAll(fd, request)) {
    body = ReadResponse(fd);
  }
  close(fd);
  return body;
}

auto VectorClient::Search(const std::string& query, int k) const
    -> std::vector<std::pair<std::string, float>> {
  const std::string json_body = "{\"query\":\"" + JsonEscape(query) +
                                "\",\"k\":" + std::to_string(k) + "}";
  const std::string body = Post("/search", json_body);
  return ParseTsv(body);
}

void VectorClient::AddDoc(const std::string& doc_id,
                          const std::string& text) const {
  const std::string truncated =
      text.size() > k_text_limit ? text.substr(0, k_text_limit) : text;
  const std::string json_body = "{\"id\":\"" + JsonEscape(doc_id) +
                                "\",\"text\":\"" + JsonEscape(truncated) + "\"}";
  Post("/index/add", json_body);
}

void VectorClient::RemoveDoc(const std::string& doc_id) const {
  Delete("/index/" + doc_id);
}

auto VectorClient::Ask(const std::string& question, int k) const
    -> std::pair<std::string, std::vector<std::string>> {
  const int fd = Connect();
  if (fd < 0) {
    return {};
  }

  // Claude API can take several seconds — extend the socket read timeout.
  constexpr int k_ask_timeout_sec = 45;
  struct timeval tv{.tv_sec = k_ask_timeout_sec, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,  // NOLINT(misc-include-cleaner)
             &tv, sizeof(tv));

  const std::string json_body = "{\"question\":\"" + JsonEscape(question) +
                                "\",\"k\":" + std::to_string(k) + "}";
  const std::string request =
      "POST /ask HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " +
      std::to_string(json_body.size()) +
      "\r\n"
      "Connection: close\r\n"
      "\r\n" +
      json_body;

  std::string body;
  if (SendAll(fd, request)) {
    body = ReadResponse(fd);
  }
  close(fd);

  if (body.empty()) {
    return {};
  }

  // Parse: {answer}\n---SOURCES---\n{file1}\n{file2}...
  constexpr std::string_view kSep = "\n---SOURCES---\n";
  const auto sep = body.find(kSep);
  const std::string answer =
      sep != std::string::npos ? body.substr(0, sep) : body;
  std::vector<std::string> sources;
  if (sep != std::string::npos) {
    std::istringstream iss(body.substr(sep + kSep.size()));
    std::string line;
    while (std::getline(iss, line)) {
      if (!line.empty()) {
        sources.push_back(line);
      }
    }
  }
  return {answer, sources};
}

}  // namespace searchserver
