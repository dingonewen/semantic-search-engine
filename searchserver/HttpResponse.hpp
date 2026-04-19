#ifndef HTTPRESPONSE_HPP_
#define HTTPRESPONSE_HPP_
#include <string>

// Builds a complete HTTP/1.1 response string ready to send over a socket.
//
// Parameters:
//   status       -- HTTP status code, e.g. 200, 404, 409
//   body         -- Response body content (HTML, plain text, binary, etc.)
//   content_type -- MIME type for the Content-Type header (default "text/plain")
//   status_text  -- Reason phrase, e.g. "Not Found". Inferred from status if empty.
//
// Returns the full response as a string including headers and body.
std::string make_response(int status,
                          const std::string& body,
                          const std::string& content_type = "text/plain",
                          const std::string& status_text = "");

#endif