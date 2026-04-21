#ifndef STATICFILE_HPP_
#define STATICFILE_HPP_
#include <string>

// Handles GET request: fines a file and returns its contents as an HTTP
// response
// files_root: the server's root directory "test_tree"
// relpath: the path from the request URL "books/artofwar.txt"
// Returns a 200 response with the file contents, or 404 if not found.
std::string StaticGet(const std::string& files_root,
                      const std::string& relpath);

// Creates or replaces a file under files_root with the given data.
// files_root: the server's root directory "test_tree"
// relpath: the path from the request URL "books/artofwar.txt"
// data: raw bytes to write as the file contents
// overwrite: if false and the file already exists, returns 409 Conflict
// Returns a 201 Created, 200 OK, or error HTTP response string.
std::string StaticPut(const std::string& files_root,
                      const std::string& relpath,
                      const std::string& data,
                      bool overwrite);

// Deletes a file under files_root.
// files_root: root directory the server is serving from
// relpath: path relative to files_root of the file to delete
// Returns a 200 OK response, or 404 if the file does not exist.
std::string StaticDelete(const std::string& files_root,
                         const std::string& relpath);

#endif