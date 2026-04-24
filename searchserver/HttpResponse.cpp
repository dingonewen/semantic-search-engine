#include "./HttpResponse.hpp"

#include <string>

/*
Given a status code, body, content type, and optional status text, it assembles
the response in the correct format: HTTP/1.1 200 OK\r\n Content-Type:
text/html\r\n Content-Length: 42\r\n
\r\n
<the body here>
*/
auto MakeResponse(int status,
                  const std::string& body,
                  const std::string& content_type,
                  const std::string& status_text) -> std::string {
  std::string reason = status_text;
  if (reason.empty()) {
    if (status == k_http_ok) {  // 200
      reason = "OK";
    } else if (status == k_http_created) {  // 201
      reason = "Created";
    } else if (status == k_http_forbidden) {  // 403
      reason = "Forbidden";
    } else if (status == k_http_not_found) {  // 404
      reason = "Not Found";
    } else if (status == k_http_conflict) {  // 409
      reason = "Conflict";
    } else if (status == k_http_not_implemented) {  // 501
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
