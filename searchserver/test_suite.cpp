#define CATCH_CONFIG_MAIN
#include "./catch.hpp"

// HttpServer::run() is not unit-testable (blocking accept loop);
// tested via manual integration: ./searchserver <port> test_tree