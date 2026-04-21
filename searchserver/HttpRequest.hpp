#ifndef HTTPREQUEST_HPP_
#define HTTPREQUEST_HPP_

#include <string>
#include <unordered_map>
#include <vector>

// Parsed HTTP request: method, path, query string, headers, and body.
struct Request {
  std::string method;
  std::string path;
  std::string query;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

// Parse raw HTTP request bytes (up to and including blank line) into a Request.
// Returns a default-constructed Request on failure.
auto ParseRequest(const std::string& raw) -> Request;

// Split URL query string (e.g. "terms=hello+world") into lowercase terms.
auto SplitTerms(const std::string& query) -> std::vector<std::string>;

#endif
