#ifndef STATICFILE_HPP_
#define STATICFILE_HPP_
#include <string>

// Handles GET request: fines a file and returns its contents as an HTTP
// response
// files_root: the server's root directory "test_tree"
// relpath: the path from the request URL "books/artofwar.txt"
// Returns a 200 response with the file contents, or 404 if not found.
auto StaticGet(const std::string& files_root, const std::string& relpath)
    -> std::string;

// Creates or replaces a file under files_root with the given data.
// files_root: the server's root directory "test_tree"
// relpath: the path from the request URL "books/artofwar.txt"
// data: raw bytes to write as the file contents
// overwrite == True if PUT, overwrite == False if POST
// (guard triggers 409 if file exists and overwrite == False)
// Returns a 201 Created, 200 OK or HTTP error.
auto StaticPut(const std::string& files_root,
               const std::string& relpath,
               const std::string& data,
               bool overwrite) -> std::string;

// Deletes a file under files_root.
// files_root: root directory the server is serving from
// relpath: path relative to files_root of the file to delete
// Returns a 204 No Content response or HTTP error
auto StaticDelete(const std::string& files_root, const std::string& relpath)
    -> std::string;

#endif