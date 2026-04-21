#include "./catch.hpp"
#include "./HttpRequest.hpp"

TEST_CASE("ParseRequest basic GET with query", "[HttpRequest]") {
    std::string raw =
        "GET /query?terms=hello+world HTTP/1.1\r\n"
        "Host: localhost:5950\r\n"
        "Connection: close\r\n"
        "\r\n";
    Request req = ParseRequest(raw);
    REQUIRE(req.method == "GET");
    REQUIRE(req.path == "/query");
    REQUIRE(req.query == "terms=hello+world");
    REQUIRE(req.headers["host"] == "localhost:5950");
    REQUIRE(req.headers["connection"] == "close");
}

TEST_CASE("ParseRequest no query string", "[HttpRequest]") {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:5950\r\n"
        "\r\n";
    Request req = ParseRequest(raw);
    REQUIRE(req.method == "GET");
    REQUIRE(req.path == "/");
    REQUIRE(req.query == "");
}

TEST_CASE("SplitTerms plus separator", "[HttpRequest]") {
    auto terms = SplitTerms("terms=hello+world");
    REQUIRE(terms.size() == 2);
    REQUIRE(terms[0] == "hello");
    REQUIRE(terms[1] == "world");
}

TEST_CASE("SplitTerms percent20 and uppercase", "[HttpRequest]") {
    auto terms = SplitTerms("terms=HELLO%20World");
    REQUIRE(terms.size() == 2);
    REQUIRE(terms[0] == "hello");
    REQUIRE(terms[1] == "world");
}
