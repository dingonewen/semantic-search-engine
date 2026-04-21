#include "./HttpResponse.hpp"

#include <string>

auto MakeResponse(int status,
                  const std::string& body,
                  const std::string& content_type,
                  const std::string& status_text) -> std::string {
  std::string reason = status_text;
  if (reason.empty()) {
    if (status == k_http_ok) {
      reason = "OK";
    } else if (status == k_http_created) {
      reason = "Created";
    } else if (status == k_http_forbidden) {
      reason = "Forbidden";
    } else if (status == k_http_not_found) {
      reason = "Not Found";
    } else if (status == k_http_conflict) {
      reason = "Conflict";
    } else if (status == k_http_not_implemented) {
      reason = "Not Implemented";
    } else {
      reason = "Unknown";
    }
  }

  std::string response =
      "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
  response += "Content-Type: " + content_type + "\r\n";
  response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  response += "\r\n";
  response += body;
  return response;
}
