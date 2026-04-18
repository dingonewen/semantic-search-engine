// ClientShell.hpp
// Simple CLI client to interact with the searchserver (get, post, put, delete).

#pragma once

#include <string>

class ClientShell {
 public:
  ClientShell(const std::string& server_addr, uint16_t port);
  ~ClientShell();

  // Run interactive loop (reads stdin until EOF)
  void run();
};
