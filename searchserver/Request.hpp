#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct Request {
  std::string method;
  std::string path;
  std::string query;
  std::unordered_map<std::string, std::string> headers;
};

Request parse_request(const std::string& raw);
std::vector<std::string> split_terms(const std::string& query);
