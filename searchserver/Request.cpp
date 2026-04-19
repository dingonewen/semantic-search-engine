#include "Request.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

static std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c == '+')
      out.push_back(' ');
    else if (c == '%' && i + 2 < s.size()) {
      auto hex = s.substr(i + 1, 2);
      char val = static_cast<char>(std::stoi(hex, nullptr, 16));
      out.push_back(val);
      i += 2;
    } else
      out.push_back(c);
  }
  return out;
}

Request parse_request(const std::string& raw) {
  Request r;
  std::istringstream in(raw);
  std::string line;
  if (!std::getline(in, line))
    return r;
  // strip \r
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  std::istringstream ls(line);
  ls >> r.method;
  std::string target;
  ls >> target;
  auto qpos = target.find('?');
  if (qpos == std::string::npos)
    r.path = target;
  else {
    r.path = target.substr(0, qpos);
    r.query = target.substr(qpos + 1);
  }

  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break;
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string k = line.substr(0, colon);
      std::string v = line.substr(colon + 1);
      // trim
      while (!v.empty() && std::isspace((unsigned char)v.front()))
        v.erase(v.begin());
      while (!v.empty() && std::isspace((unsigned char)v.back()))
        v.pop_back();
      r.headers[k] = v;
    }
  }
  // parse query terms name=q or terms=
  return r;
}

std::vector<std::string> split_terms(const std::string& query) {
  std::vector<std::string> out;
  if (query.empty())
    return out;
  // allow both q=... and terms=...
  size_t pos = query.find('=');
  std::string val = query;
  if (pos != std::string::npos)
    val = query.substr(pos + 1);
  std::string decoded = url_decode(val);
  std::istringstream ss(decoded);
  std::string w;
  while (ss >> w) {
    // lowercase
    for (auto& c : w)
      c = static_cast<char>(std::tolower((unsigned char)c));
    out.push_back(w);
  }
  return out;
}
