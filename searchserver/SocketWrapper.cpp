#include "./SocketWrapper.hpp"

SocketWrapper::SocketWrapper(): fd_(-1) {}  // when socket is invalid; use initializer list instead

// Create from an existing file descriptor
SocketWrapper::SocketWrapper(int fd) : fd_(fd) {}

SocketWrapper::SocketWrapper(SocketWrapper&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

SocketWrapper& SocketWrapper::operator=(SocketWrapper&& other) noexcept {
    if (this != &other) {
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

SocketWrapper::~SocketWrapper() {

}

int SocketWrapper::fd() const noexcept {

}

bool SocketWrapper::valid() const noexcept {

}

// factory function which creates a brand new SocketWrapper from scratch (binds and listens on a port)
SocketWrapper SocketWrapper::listen_on(const std::string& addr, uint16_t port) {

}

SocketWrapper SocketWrapper::accept() const {

}

// send/recv helpers (simple wrappers)
ssize_t SocketWrapper::send_all(const char* data, size_t len) {

}

ssize_t SocketWrapper::recv_some(char* buf, size_t len) {

}