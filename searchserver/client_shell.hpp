#pragma once
#include <string>

struct ClientConfig {
  std::string host = "127.0.0.1";
  uint16_t port = 5950;
};

int run_client_shell(const ClientConfig& cfg);
