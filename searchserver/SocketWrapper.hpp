// SocketWrapper.hpp
// Lightweight RAII wrapper for POSIX sockets used by the searchserver project.

#pragma once

#include <sys/socket.h>
#include <string>

class SocketWrapper {
 public:
  // Create an invalid socket
  SocketWrapper();
  // Create from an existing file descriptor
  explicit SocketWrapper(int fd);
  SocketWrapper(const SocketWrapper&) = delete;
  SocketWrapper& operator=(const SocketWrapper&) = delete;
  SocketWrapper(SocketWrapper&& other) noexcept;
  SocketWrapper& operator=(SocketWrapper&& other) noexcept;
  ~SocketWrapper();

  int fd() const noexcept;
  bool valid() const noexcept;

  // bind, listen, accept helpers
  static SocketWrapper listen_on(const std::string& addr, uint16_t port);
  SocketWrapper accept() const;

  // send/recv helpers (simple wrappers)
  ssize_t send_all(const char* data, size_t len);
  ssize_t recv_some(char* buf, size_t len);

 private:
  int fd_;
};
