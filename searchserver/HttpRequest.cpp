#include "./HttpRequest.hpp"
// these two free functions are functions for struct Request, not belong to class

#include <sstream> // istringstream
#include <algorithm> // transform

// split the raw string on \r\n, first line gives method/path/query, remaining lines give headers
/*
input:
GET /query?terms=hello+world HTTP/1.1\r\n
Host: localhost:5950\r\n
Connection: close\r\n
\r\n

output: a Request struct with
method = "GET"
path = "/query"
query = "terms=hello+world"
headers = {"host": "localhost:5950", "connection": "close"}
*/
Request parse_request(const std::string& raw) {
    Request req;
    std::istringstream ss(raw);
    std::string first_line;  // only first line has HTTP method, path, query, rest lines are all headers 
    std::getline(ss, first_line, '\n');
    // parse the first line
    std::istringstream first_ss(first_line);
    std::string method, path, query;
    std::getline(first_ss, method, ' ');
    std::getline(first_ss, path, ' ');
    std::getline(first_ss, query, ' ');
    if (method.empty() || path.empty()) {
        return req;  // failure — return default
    }
    req.method = method;
    // split path on '?'; take before ? as path, after as query
    size_t pos = path.find('?');  
    if (pos != std::string::npos) {  // if ? is found, not every request has query
        req.path  = path.substr(0, pos);
        req.query = path.substr(pos + 1);
    } else {
        req.path  = path;
        req.query = "";
    }
    // parse rest of the lines for headers map
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (!line.empty() && line.back() == '\r') {  // check \r\n
            line.pop_back();
        }
        if (line.empty()) break;  // blank line = end of headers
        size_t colon = line.find(':');  // ensure it's headers not other information
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 2);  // skip ": "
            // lowercase key since header keys are case-insensitive (values are not)
            std::transform(key.begin(), key.end(), key.begin(), ::tolower); 
            req.headers[key] = val;
        }
    }
    return req;
}

// split on + or %20, lowercase each term
/*
Input: "terms=hello+world"
Output: ["hello", "world"]
*/
std::vector<std::string> split_terms(const std::string& query) {
    std::vector<std::string> res;
    if (query.empty()) {
        return res;
    }

    std::string q = query;
    size_t eq = q.find('=');  // remove "terms=" prefix
    if (eq != std::string::npos) {
        q = q.substr(eq + 1);
    }

    size_t pos = 0;
    while ((pos = q.find("%20", pos)) != std::string::npos) {
        q.replace(pos, 3, "+");    // replace %20 with +
    }

    std::istringstream ss(q);
    std::string token;
    while (std::getline(ss, token, '+')) {  // split on + and lowercase each term
        if (!token.empty()) {
            std::transform(token.begin(), token.end(), token.begin(), ::tolower);  // search query is case-insensitive
            res.push_back(token);
        }
    }
    return res;
}