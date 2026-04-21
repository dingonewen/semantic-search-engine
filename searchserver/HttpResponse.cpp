#include "./HttpResponse.hpp"
#include <string>

// Builds a complete HTTP/1.1 response string ready to send over a socket
// Given a status code, body, and content type, it assembles the response in the
// correct HTTP format
std::string MakeResponse(int status,
                          const std::string& body,
                          const std::string& content_type,
                          const std::string& status_text) {
  std::string reason =
      status_text;  // if caller didn't pass a 4th argument, we add it to it
  if (reason.empty()) {
    if (status == 200)
      reason = "OK";
    else if (status == 201)
      reason = "Created";
    else if (status == 403)
      reason = "Forbidden";
    else if (status == 404)
      reason = "Not Found";
    else if (status == 409)
      reason = "Conflict";
    else if (status == 501)
      reason = "Not Implemented";
    else
      reason = "Unknown";
  }

  std::string response = "HTTP/1.1 " + std::to_string(status) + " " + reason +
                         "\r\n";  // don't forget \r\n
  response += "Content-Type: " + content_type + "\r\n";
  response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  response += "\r\n";  // \r\n\r\n is the separater between headers and body
  response += body;
  return response;
}