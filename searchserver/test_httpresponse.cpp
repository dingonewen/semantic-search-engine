#include "./catch.hpp"
#include "./HttpResponse.hpp"

TEST_CASE("make_response 200 OK", "[HttpResponse]") {
    std::string resp = make_response(200, "hello", "text/plain","");
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