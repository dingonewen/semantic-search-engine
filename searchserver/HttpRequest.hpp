#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// Represents a parsed HTTP request.
struct Request {
  std::string method;   // HTTP verb: "GET", "POST", "PUT", "DELETE", etc.
  std::string path;     // URL path component, e.g. "/static/books/foo.txt"
  std::string query;    // Raw query string after '?', e.g. "term1+term2"
  std::unordered_map<std::string, std::string> headers;  // Header name -> value
};

// Parses a raw HTTP request string (headers only, up to and including the blank
// line) into a Request struct. Returns a default-constructed Request on failure.
Request parse_request(const std::string& raw);

// Splits a URL query string (e.g. "hello+world" or "hello%20world") into
// individual lowercase search terms. Returns an empty vector if query is empty.
std::vector<std::string> split_terms(const std::string& query);
