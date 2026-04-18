// HttpResponse.hpp
// Minimal representation of an HTTP response (status, headers, body).

#pragma once

#include <string>
#include <unordered_map>

struct HttpResponse {
  int status_code = 200;
  std::string reason = "OK";
  std::unordered_map<std::string, std::string> headers;
  std::string body;

  std::string to_string() const;  // helper to serialize response
};
