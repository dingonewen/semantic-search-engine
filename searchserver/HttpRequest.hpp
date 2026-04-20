#ifndef HTTPREQUEST_HPP_
#define HTTPREQUEST_HPP_

#include <string>
#include <unordered_map>
#include <vector>

// Defines the Request struct and some free functions that parse raw HTTP text
// into usable data e.g. GET /query?terms=hello+world HTTP/1.1 in spec it
// assumes clients send legal HTTP/1.1 request
struct Request {
  std::string method;  // GET
  std::string path;    // /query
  std::string query;   // terms=hello+world
  std::unordered_map<std::string, std::string>
      headers;  // connection: close (case-insensitive)
};

// split the raw string on \r\n, first line gives method/path/query, remaining
// lines give headers
Request parse_request(const std::string& raw);

// split on + or %20, lowercase each term
std::vector<std::string> split_terms(const std::string& query);

#endif