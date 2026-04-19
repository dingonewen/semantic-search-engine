#include "StaticFile.hpp"
#include "Response.hpp"

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static bool file_exists(const std::string& p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static std::string read_all(const std::string& p) {
  std::ifstream in(p, std::ios::in | std::ios::binary);
  if (!in)
    return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static std::string content_type_for(const std::string& p) {
  if (p.size() >= 4 && p.substr(p.size() - 4) == ".txt")
    return "text/plain";
  if (p.size() >= 5 && p.substr(p.size() - 5) == ".html")
    return "text/html";
  return "application/octet-stream";
}

std::string serve_static(const std::string& files_root,
                         const std::string& relpath) {
  // try as-given
  std::string path = relpath;
  if (file_exists(path)) {
    return make_response(200, read_all(path), content_type_for(path));
  }
  if (!files_root.empty()) {
    std::string joined = files_root + "/" + relpath;
    if (file_exists(joined))
      return make_response(200, read_all(joined), content_type_for(joined));
  }
  // not found
  return make_response(404, "<h1>404 Not Found</h1>", "text/html");
}

static bool ensure_parent_dirs(const std::string& path) {
  try {
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
      std::filesystem::create_directories(p.parent_path());
    }
    return true;
  } catch (...) {
    return false;
  }
}

std::string static_put(const std::string& files_root,
                       const std::string& relpath,
                       const std::string& data,
                       bool overwrite) {
  try {
    std::filesystem::path candidate = relpath;
    std::filesystem::path target;
    if (!candidate.is_absolute() && std::filesystem::exists(candidate) &&
        std::filesystem::is_regular_file(candidate))
      target = candidate;
    else if (!files_root.empty()) {
      auto cand = std::filesystem::path(files_root) / relpath;
      if (std::filesystem::exists(cand) &&
          std::filesystem::is_regular_file(cand))
        target = cand;
      else
        target = cand;  // we'll create
    } else
      target = std::filesystem::current_path() / relpath;

    // ensure the resulting path is within files_root if files_root provided
    if (!files_root.empty()) {
      auto can_root = std::filesystem::canonical(files_root);
      std::filesystem::path can_target_parent =
          std::filesystem::weakly_canonical(target);
      auto s = can_target_parent.string();
      auto r = can_root.string();
      if (s.size() < r.size() || s.compare(0, r.size(), r) != 0) {
        return make_response(403, "<h1>403 Forbidden</h1>", "text/html",
                             "Forbidden");
      }
    }

    bool exists = std::filesystem::exists(target);
    if (exists && !overwrite) {
      return make_response(409, "<h1>409 Conflict</h1>", "text/html",
                           "Conflict");
    }

    if (!ensure_parent_dirs(target.string()))
      return make_response(500, "<h1>500 Internal Server Error</h1>",
                           "text/html", "Internal Server Error");

    std::ofstream out(target.string(),
                      std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out)
      return make_response(500, "<h1>500 Internal Server Error</h1>",
                           "text/html", "Internal Server Error");
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.close();

    if (exists)
      return make_response(200, "", "text/plain", "OK");
    else
      return make_response(201, "", "text/plain", "Created");
  } catch (...) {
    return make_response(500, "<h1>500 Internal Server Error</h1>", "text/html",
                         "Internal Server Error");
  }
}

std::string static_delete(const std::string& files_root,
                          const std::string& relpath) {
  try {
    std::filesystem::path target = relpath;
    if (!target.is_absolute())
      target = std::filesystem::path(files_root) / relpath;
    if (!std::filesystem::exists(target) ||
        !std::filesystem::is_regular_file(target))
      return make_response(404, "<h1>404 Not Found</h1>", "text/html",
                           "Not Found");
    // ensure inside files_root
    if (!files_root.empty()) {
      auto can_root = std::filesystem::canonical(files_root);
      auto can_target = std::filesystem::canonical(target);
      auto s = can_target.string();
      auto r = can_root.string();
      if (s.size() < r.size() || s.compare(0, r.size(), r) != 0)
        return make_response(403, "<h1>403 Forbidden</h1>", "text/html",
                             "Forbidden");
    }
    std::error_code ec;
    bool ok = std::filesystem::remove(target, ec);
    if (!ok || ec)
      return make_response(500, "<h1>500 Internal Server Error</h1>",
                           "text/html", "Internal Server Error");
    return make_response(204, "", "text/plain", "No Content");
  } catch (...) {
    return make_response(500, "<h1>500 Internal Server Error</h1>", "text/html",
                         "Internal Server Error");
  }
}
