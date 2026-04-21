#include "./HttpRequest.hpp"

#include <algorithm>  // std::ranges::transform
#include <cctype>     // ::tolower
#include <cstddef>    // size_t
#include <sstream>    // std::istringstream
#include <string>
#include <vector>

auto ParseRequest(const std::string& raw) -> Request {
  Request req;
  std::istringstream ss(raw);
  std::string first_line;
  std::getline(ss, first_line, '\n');

  std::istringstream first_ss(first_line);
  std::string method;
  std::string path;
  std::string version;
  std::getline(first_ss, method, ' ');
  std::getline(first_ss, path, ' ');
  std::getline(first_ss, version, ' ');

  if (method.empty() || path.empty()) {
    return req;
  }
  req.method = method;

  const size_t pos = path.find('?');
  if (pos != std::string::npos) {
    req.path = path.substr(0, pos);
    req.query = path.substr(pos + 1);
  } else {
    req.path = path;
    req.query = "";
  }

  std::string line;
  while (std::getline(ss, line, '\n')) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    const size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = line.substr(0, colon);
      const std::string val = line.substr(colon + 2);
      std::ranges::transform(key, key.begin(), ::tolower);
      req.headers[key] = val;
    }
  }

  const auto it = req.headers.find("content-length");
  if (it != req.headers.end()) {
    const size_t body_len = std::stoul(it->second);
    const size_t sep = raw.find("\r\n\r\n");
    if (sep != std::string::npos) {
      req.body = raw.substr(sep + 4, body_len);
    }
  }

  return req;
}

auto SplitTerms(const std::string& query) -> std::vector<std::string> {
  std::vector<std::string> res;
  if (query.empty()) {
    return res;
  }

  std::string q = query;
  const size_t eq = q.find('=');
  if (eq != std::string::npos) {
    q = q.substr(eq + 1);
  }

  size_t pos = 0;
  while ((pos = q.find("%20", pos)) != std::string::npos) {
    q.replace(pos, 3, "+");
  }

  std::istringstream ss(q);
  std::string token;
  while (std::getline(ss, token, '+')) {
    if (!token.empty()) {
      std::ranges::transform(token, token.begin(), ::tolower);
      res.push_back(token);
    }
  }
  return res;
}
