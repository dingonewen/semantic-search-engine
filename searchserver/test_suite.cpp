#define CATCH_CONFIG_MAIN
#include "./catch.hpp"
#include "./HttpRequest.hpp"
#include "./HttpResponse.hpp"

TEST_CASE("parse_request basic GET with query", "[HttpRequest]") {
    std::string raw =
        "GET /query?terms=hello+world HTTP/1.1\r\n"
        "Host: localhost:5950\r\n"
        "Connection: close\r\n"
        "\r\n";
    Request req = parse_request(raw);
    REQUIRE(req.method == "GET");
    REQUIRE(req.path == "/query");
    REQUIRE(req.query == "terms=hello+world");
    REQUIRE(req.headers["host"] == "localhost:5950");
    REQUIRE(req.headers["connection"] == "close");
}

TEST_CASE("parse_request no query string", "[HttpRequest]") {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:5950\r\n"
        "\r\n";
    Request req = parse_request(raw);
    REQUIRE(req.method == "GET");
    REQUIRE(req.path == "/");
    REQUIRE(req.query == "");
}

TEST_CASE("split_terms plus separator", "[HttpRequest]") {
    auto terms = split_terms("terms=hello+world");
    REQUIRE(terms.size() == 2);
    REQUIRE(terms[0] == "hello");
    REQUIRE(terms[1] == "world");
}

TEST_CASE("split_terms percent20 and uppercase", "[HttpRequest]") {
    auto terms = split_terms("terms=HELLO%20World");
    REQUIRE(terms.size() == 2);
    REQUIRE(terms[0] == "hello");
    REQUIRE(terms[1] == "world");
}

TEST_CASE("make_response 200 OK", "[HttpResponse]") {
    std::string resp = make_response(200, "hello", "text/plain");
    REQUIRE(resp.find("HTTP/1.1 200 OK") != std::string::npos);
    REQUIRE(resp.find("Content-Type: text/plain") != std::string::npos);
    REQUIRE(resp.find("Content-Length: 5") != std::string::npos);
    REQUIRE(resp.find("hello") != std::string::npos);
}

TEST_CASE("make_response 404 custom status_text", "[HttpResponse]") {
    std::string resp = make_response(404, "not found", "text/html", "Not Found");
    REQUIRE(resp.find("HTTP/1.1 404 Not Found") != std::string::npos);
    REQUIRE(resp.find("Content-Type: text/html") != std::string::npos);
    REQUIRE(resp.find("Content-Length: 9") != std::string::npos);
}