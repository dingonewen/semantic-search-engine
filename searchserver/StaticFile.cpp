#include "StaticFile.hpp"
#include "HttpResponse.hpp"

#include <filesystem>
#include <fstream>

// helper functions

// Takes a file path p and returns the full file contents as a std::string
static std::string ReadAll(const std::string& p) {
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
static std::string ContentTypeFor(const std::string& p) {
  // broswer displays raw text
  if (p.size() >= 4 && p.substr(p.size() - 4) == ".txt")
    return "text/plain";
  // broswer renders it as a webpage
  if (p.size() >= 5 && p.substr(p.size() - 5) == ".html")
    return "text/html";
  // browser treats it as a generic binary download
  return "application/octet-stream";
}

// Returns a 403 response string if target is outside files_root, else return ""
static std::string CheckWithinRoot(const std::string& files_root,
                                   const std::filesystem::path& target) {
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
      can_target.compare(0, can_root.size(), can_root) != 0 ||
      (can_target.size() > can_root.size() &&
       can_target[can_root.size()] != '/')) {
    return MakeResponse(403, "<h1>403 Forbidden</h1>", "text/html",
                        "Forbidden");
  }
  return "";
}

auto StaticGet(const std::string& files_root, const std::string& relpath)
    -> std::string {
  // Check path traversal BEFORE existence check
  std::string error1 = CheckWithinRoot(files_root, relpath);
  if (!error1.empty()) {
    return error1;
  }

  if (std::filesystem::is_regular_file(relpath)) {
    return MakeResponse(200, ReadAll(relpath), ContentTypeFor(relpath));
  }
  // not found
  return MakeResponse(404, "<h1>404 Not Found</h1>", "text/html");
}

// Writes data to target. Returns a 500 response on failure, else ""
static std::string WriteFile(const std::filesystem::path& target,
                             const std::string& data) {
  // write in raw bytes and truncate existing content on open
  std::ofstream out(target.string(), std::ios::binary | std::ios::trunc);
  if (!out) {
    return MakeResponse(500, "<h1>500 Internal Server Error</h1>", "text/html",
                        "Internal Server Error");
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return "";
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
      return MakeResponse(409, "<h1>409 Conflict</h1>", "text/html",
                          "Conflict");
    }
    // Check if the directory containing the target file actually exists
    // auto parent = target.parent_path();
    // if (!parent.empty() && !std::filesystem::is_directory(parent)) {
    //   return MakeResponse(400, "<h1>400 Bad Request</h1>", "text/html",
    //                       "Invalid Request");
    // }
    std::string error2 = WriteFile(relpath, data);
    if (!error2.empty()) {
      return error2;
    }
    return exist ? MakeResponse(200, "", "text/plain", "OK")
                 : MakeResponse(201, "", "text/plain", "Created");
  } catch (const std::exception&
               e) {  // std::filesystem::canonical throws filesystem_error if
                     // files_root doesn't exist
    return MakeResponse(500, "<h1>500 Internal Server Error</h1>", "text/html",
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
      return MakeResponse(404, "<h1>404 Not Found</h1>", "text/html",
                          "Not Found");
    }

    // Delete the file
    std::error_code ec;
    bool success = std::filesystem::remove(relpath, ec);
    if (!success || ec) {
      return MakeResponse(500, "<h1>500 Internal Server Error</h1>",
                          "text/html", "Internal Server Error");
    }
    // Delete successfully
    return MakeResponse(204, "", "text/plain", "No Content");
  } catch (const std::exception&
               e) {  // std::filesystem::canonical throws filesystem_error if
                     // files_root doesn't exist
    return MakeResponse(500, "<h1>500 Internal Server Error</h1>", "text/html",
                        "Internal Server Error");
  }
}
