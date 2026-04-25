#include "StaticFile.hpp"
#include "HttpResponse.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>

namespace {

// Takes a file path p and returns the full file contents as a std::string
auto ReadAll(const std::string& p) -> std::string {
  // Opens the file at path p for reading in binary mode so that bytes are read
  // exactily as-is
  std::ifstream in(p, std::ios::binary);
  std::string res;
  char c;
  while (in.get(c)) {
    res += c;
  }
  return res;
}

// Tells the browser how to interprete the bytes it receives
auto ContentTypeFor(const std::string& p) -> std::string {
  // broswer displays raw text
  if (p.ends_with(".txt"))
    return "text/plain";
  // broswer renders it as a webpage
  if (p.ends_with(".html"))
    return "text/html";
  // browser treats it as a generic binary download
  return "application/octet-stream";
}

// Returns a 403 response string if target is outside files_root, else ""
auto CheckWithinRoot(const std::string& files_root,
                     const std::filesystem::path& target) -> std::string {
  if (files_root.empty()) {
    return "";
  }
  auto can_root = std::filesystem::canonical(files_root).string();
  auto can_target = std::filesystem::weakly_canonical(target).string();
  // invalid access:
  // can_root = "/home/moya/.../searchserver/test_tree"
  // can_target = "/home/moya/.../parallel" or
  // can_target = "/home/moya/.../searchserver/test_file/file.txt" or
  // can_target = "/home/moya/.../searchserver/test_file_evil/file.txt"
  if (can_target.size() < can_root.size() ||
      !can_target.starts_with(can_root) ||
      (can_target.size() > can_root.size() &&
       can_target[can_root.size()] != '/')) {
    return MakeResponse(k_http_forbidden, "<h1>403 Forbidden</h1>", "text/html",
                        "Forbidden");
  }
  return "";
}

// Writes data to target. Returns a 500 response on failure, else ""
auto WriteFile(const std::filesystem::path& target, const std::string& data)
    -> std::string {
  // write in raw bytes and truncate existing content on open
  std::ofstream out(target.string(), std::ios::binary | std::ios::trunc);
  if (!out) {
    return MakeResponse(k_http_internal_error,
                        "<h1>500 Internal Server Error</h1>", "text/html",
                        "Internal Server Error");
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return "";
}

}  // namespace

auto StaticGet(const std::string& files_root, const std::string& relpath)
    -> std::string {
  // Check path traversal BEFORE existence check
  std::string error1 = CheckWithinRoot(files_root, relpath);
  if (!error1.empty()) {
    return error1;
  }
  if (std::filesystem::is_regular_file(relpath)) {
    return MakeResponse(k_http_ok, ReadAll(relpath), ContentTypeFor(relpath));
  }
  // not found
  return MakeResponse(k_http_not_found, "<h1>404 Not Found</h1>", "text/html");
}

auto StaticPut(const std::string& files_root,
               const std::string& relpath,
               const std::string& data,
               bool overwrite) -> std::string {
  try {
    // Check if the client request to access any files outside of the
    // root directory
    std::string error1 = CheckWithinRoot(files_root, relpath);
    if (!error1.empty()) {
      return error1;
    }
    // POST: check if the file already exists. Return 409 Conflict on POST
    // existing file
    auto exist = std::filesystem::exists(relpath);
    if (exist && !overwrite) {
      return MakeResponse(k_http_conflict, "<h1>409 Conflict</h1>", "text/html",
                          "Conflict");
    }
    std::string error2 = WriteFile(relpath, data);
    if (!error2.empty()) {
      return error2;
    }
    return exist ? MakeResponse(k_http_ok, "", "text/plain", "OK")
                 : MakeResponse(k_http_created, "", "text/plain", "Created");
  } catch (const std::exception&
               e) {  // std::filesystem::canonical throws filesystem_error if
                     // files_root doesn't exist
    return MakeResponse(k_http_internal_error,
                        "<h1>500 Internal Server Error</h1>", "text/html",
                        "Internal Server Error");
  }
}

auto StaticDelete(const std::string& files_root, const std::string& relpath)
    -> std::string {
  try {
    // Check path traversal BEFORE existence check
    std::string error1 = CheckWithinRoot(files_root, relpath);
    if (!error1.empty()) {
      return error1;
    }
    if (!std::filesystem::exists(relpath) ||
        !std::filesystem::is_regular_file(relpath)) {
      return MakeResponse(k_http_not_found, "<h1>404 Not Found</h1>",
                          "text/html", "Not Found");
    }
    // Delete the file
    std::error_code ec;
    const bool success = std::filesystem::remove(relpath, ec);
    if (!success || ec) {
      return MakeResponse(k_http_internal_error,
                          "<h1>500 Internal Server Error</h1>", "text/html",
                          "Internal Server Error");
    }
    // Delete successfully
    return MakeResponse(k_http_no_content, "", "text/plain", "No Content");
  } catch (const std::exception&
               e) {  // std::filesystem::canonical throws filesystem_error if
                     // files_root doesn't exist
    return MakeResponse(k_http_internal_error,
                        "<h1>500 Internal Server Error</h1>", "text/html",
                        "Internal Server Error");
  }
}
