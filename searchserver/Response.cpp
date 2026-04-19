#include "Response.hpp"
#include <sstream>

static std::string default_status_text(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 409:
      return "Conflict";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    default:
      return "Unknown";
  }
}

std::string make_response(int status,
                          const std::string& body,
                          const std::string& content_type,
                          const std::string& status_text) {
  std::string text =
      status_text.empty() ? default_status_text(status) : status_text;
  std::ostringstream out;
  out << "HTTP/1.1 " << status << " " << text << "\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Content-Type: " << content_type << "\r\n";
  out << "Connection: close\r\n";
  out << "\r\n";
  out << body;
  return out.str();
}
