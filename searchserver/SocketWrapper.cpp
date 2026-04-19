#include <unistd.h>  // close()
#include <iostream>
#include <cstring>


#include "./SocketWrapper.hpp"

SocketWrapper::SocketWrapper(): fd_(-1) {}  // when socket is invalid; use initializer list instead

// Create from an existing file descriptor
// caller's responsibility to ensure that the right fd_ was passed in
SocketWrapper::SocketWrapper(int fd) : fd_(fd) {}

// move constructor
SocketWrapper::SocketWrapper(SocketWrapper&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

// move assignment
SocketWrapper& SocketWrapper::operator=(SocketWrapper&& other) noexcept {
    if (this != &other) {
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

SocketWrapper::~SocketWrapper() {
    if (fd_ >= 0) {  // prevents closing an invalid/already-moved socket
        close(fd_);  // calling close(-1) is error
    }
}

int SocketWrapper::fd() const noexcept {
    return fd_;
}

bool SocketWrapper::valid() const noexcept {
    return fd_ >= 0;
}

// factory function which creates a brand new SocketWrapper from scratch (binds and listens on a port)
SocketWrapper SocketWrapper::listen_on(const std::string& addr, uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (socket_fd == -1) {
     std::cerr << strerror(errno) << std::endl;
     return EXIT_FAILURE; 
  }
  close(socket_fd);
  return EXIT_SUCCESS;
}


SocketWrapper SocketWrapper::accept() const {

}

// send/recv helpers (simple wrappers)
ssize_t SocketWrapper::send_all(const char* data, size_t len) {

}

ssize_t SocketWrapper::recv_some(char* buf, size_t len) {

}