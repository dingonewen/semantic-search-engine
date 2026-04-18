// ClientConnection.hpp
// Represents a single client connection: reading requests and writing
// responses.

#pragma once

#include <memory>
#include <string>

class SocketWrapper;
class HttpRequest;
class HttpResponse;

class ClientConnection {
 public:
  explicit ClientConnection(SocketWrapper sock);
  ~ClientConnection();

  // Read the next request from the client. Returns nullptr on EOF/closed.
  std::unique_ptr<HttpRequest> read_request();

  // Write a response back to the client
  bool write_response(const HttpResponse& resp);

  // Peer address/port for logging
  std::string peer_address() const;

 private:
  SocketWrapper sock_;
};
