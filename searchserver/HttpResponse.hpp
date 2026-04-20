#ifndef HTTPRESPONSE_HPP_
#define HTTPRESPONSE_HPP_
#include <string>

// Builds a complete HTTP/1.1 response string ready to send over a socket.
/* Given a status code, body, and content type, it assembles the response in the correct HTTP format:
HTTP/1.1 200 OK\r\n
Content-Type: text/html\r\n
Content-Length: 42\r\n
\r\n
<html>...body here...</html>
*/
// Returns the full response as a string including headers and body
// caller can omit 3rd and 4th argument
std::string make_response(int status,
                          const std::string& body,
                          const std::string& content_type = "text/plain",
                          const std::string& status_text = "");

#endif