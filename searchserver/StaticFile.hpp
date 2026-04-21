#ifndef STATICFILE_HPP_
#define STATICFILE_HPP_
#include <string>

// Reads a file from disk and returns a complete HTTP response string.
// files_root: root directory the server is serving from
// relpath: path relative to files_root (e.g. "books/foo.txt")
// Returns a 200 response with the file contents, or 404 if not found.
std::string serve_static(const std::string& files_root,
                         const std::string& relpath);

// Creates or replaces a file under files_root with the given data.
// files_root: root directory the server is serving from
// relpath: destination path relative to files_root
// data: raw bytes to write as the file contents
// overwrite: if false and the file already exists, returns 409 Conflict
// Returns a 201 Created, 200 OK, or error HTTP response string.
std::string static_put(const std::string& files_root,
                       const std::string& relpath,
                       const std::string& data,
                       bool overwrite);

// Deletes a file under files_root.
// files_root: root directory the server is serving from
// relpath: path relative to files_root of the file to delete
// Returns a 200 OK response, or 404 if the file does not exist.
std::string static_delete(const std::string& files_root,
                          const std::string& relpath);

#endif