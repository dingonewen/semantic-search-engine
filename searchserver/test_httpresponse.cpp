#include "./catch.hpp"
#include "./HttpResponse.hpp"

TEST_CASE("MakeResponse 200 OK", "[HttpResponse]") {
    std::string resp = MakeResponse(200, "hello", "text/plain","");
    REQUIRE(resp.find("HTTP/1.1 200 OK") != std::string::npos);
    REQUIRE(resp.find("Content-Type: text/plain") != std::string::npos);
    REQUIRE(resp.find("Content-Length: 5") != std::string::npos);
    REQUIRE(resp.find("hello") != std::string::npos);
}

TEST_CASE("MakeResponse 404 custom status_text", "[HttpResponse]") {
    std::string resp = MakeResponse(404, "not found", "text/html", "Not Found");
    REQUIRE(resp.find("HTTP/1.1 404 Not Found") != std::string::npos);
    REQUIRE(resp.find("Content-Type: text/html") != std::string::npos);
    REQUIRE(resp.find("Content-Length: 9") != std::string::npos);
}

TEST_CASE("MakeResponse default status text for known codes", "[HttpResponse]") {
    REQUIRE(MakeResponse(200, "").find("HTTP/1.1 200 OK") != std::string::npos);
    REQUIRE(MakeResponse(201, "").find("HTTP/1.1 201 Created") != std::string::npos);
    REQUIRE(MakeResponse(403, "").find("HTTP/1.1 403 Forbidden") != std::string::npos);
    REQUIRE(MakeResponse(404, "").find("HTTP/1.1 404 Not Found") != std::string::npos);
}