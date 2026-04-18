// HttpRequest.hpp
// Minimal representation of an HTTP request (headers + body + method + path).

#pragma once

#include <string>
#include <unordered_map>

struct HttpRequest {
  std::string method;
  std::string path;
  std::string http_version;  // e.g. "HTTP/1.1"
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};
