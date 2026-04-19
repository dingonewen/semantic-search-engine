#pragma once
#include <string>

std::string serve_static(const std::string& files_root,
                         const std::string& relpath);

// upload or replace a resource under files_root. If overwrite==false and the
// file already exists, returns a 409 Conflict response. Returns an HTTP
// response string.
std::string static_put(const std::string& files_root,
                       const std::string& relpath,
                       const std::string& data,
                       bool overwrite);

// delete a resource under files_root. Returns an HTTP response string.
std::string static_delete(const std::string& files_root,
                          const std::string& relpath);
