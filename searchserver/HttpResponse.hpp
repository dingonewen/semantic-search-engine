#ifndef HTTPRESPONSE_HPP_
#define HTTPRESPONSE_HPP_

#include <string>

// HTTP status code constants
static constexpr int k_http_ok = 200;
static constexpr int k_http_created = 201;
static constexpr int k_http_forbidden = 403;
static constexpr int k_http_not_found = 404;
static constexpr int k_http_conflict = 409;
static constexpr int k_http_not_implemented = 501;

// Build a complete HTTP/1.1 response string ready to send over a socket.
auto MakeResponse(int status,
                  const std::string& body,
                  const std::string& content_type = "text/plain",
                  const std::string& status_text = "") -> std::string;

#endif
