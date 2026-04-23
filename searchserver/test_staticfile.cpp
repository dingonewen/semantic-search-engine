#include "./StaticFile.hpp"
#include "./catch.hpp"

#include <filesystem>
#include <fstream>
#include <string>

// files_root for all tests
static const std::string kRoot = "test_tree";

// ---------------------------------------------------------------------------
// StaticGet
// ---------------------------------------------------------------------------

TEST_CASE("StaticGet 200 for buffalo.txt", "[Test_StaticFile]") {
  std::string resp = StaticGet(kRoot, "tiny/buffalo.txt");
  REQUIRE(resp.find("HTTP/1.1 200") != std::string::npos);
  REQUIRE(resp.find("Content-Type: text/plain") != std::string::npos);
  REQUIRE(resp.find("Buffalo") != std::string::npos);
}

TEST_CASE("StaticGet 200 for home-on-the-range.txt", "[Test_StaticFile]") {
  std::string resp = StaticGet(kRoot, "tiny/home-on-the-range.txt");
  REQUIRE(resp.find("HTTP/1.1 200") != std::string::npos);
  REQUIRE(resp.find("Content-Type: text/plain") != std::string::npos);
  REQUIRE(resp.find("buffalo roam") != std::string::npos);
}

TEST_CASE("StaticGet 404 for missing file", "[Test_StaticFile]") {
  std::string resp = StaticGet(kRoot, "tiny/no_such_file.txt");
  REQUIRE(resp.find("HTTP/1.1 404") != std::string::npos);
}

// ---------------------------------------------------------------------------
// StaticPut
// ---------------------------------------------------------------------------

TEST_CASE("StaticPut creates new file in tiny/ and returns 201",
          "[Test_StaticFile]") {
  std::string resp = StaticPut(kRoot, "tiny/put_new.txt", "hello", true);
  REQUIRE(resp.find("HTTP/1.1 201") != std::string::npos);
  REQUIRE(std::filesystem::exists("test_tree/tiny/put_new.txt"));
  std::filesystem::remove("test_tree/tiny/put_new.txt");
}

TEST_CASE("StaticPut overwrites existing file and returns 200",
          "[Test_StaticFile]") {
  // First create a scratch file
  std::filesystem::copy_file("test_tree/tiny/buffalo.txt",
                             "test_tree/tiny/scratch.txt",
                             std::filesystem::copy_options::overwrite_existing);

  std::string resp = StaticPut(kRoot, "tiny/scratch.txt", "new content", true);
  REQUIRE(resp.find("HTTP/1.1 200") != std::string::npos);

  std::ifstream in("test_tree/tiny/scratch.txt");
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  REQUIRE(content == "new content");
  std::filesystem::remove("test_tree/tiny/scratch.txt");
}

TEST_CASE("StaticPut POST 409 conflict on existing file", "[Test_StaticFile]") {
  // buffalo.txt exists → POST (overwrite=false) should 409
  std::string resp = StaticPut(kRoot, "tiny/buffalo.txt", "data", false);
  REQUIRE(resp.find("HTTP/1.1 409") != std::string::npos);
}

TEST_CASE("StaticPut 403 for path traversal", "[Test_StaticFile]") {
  std::string resp = StaticPut(kRoot, "../../evil.txt", "bad", true);
  REQUIRE(resp.find("HTTP/1.1 403") != std::string::npos);
}

// ---------------------------------------------------------------------------
// StaticDelete
// ---------------------------------------------------------------------------

TEST_CASE("StaticDelete removes file and returns 204", "[Test_StaticFile]") {
  // Create a scratch file to delete
  std::filesystem::copy_file("test_tree/tiny/buffalo.txt",
                             "test_tree/tiny/to_delete.txt",
                             std::filesystem::copy_options::overwrite_existing);

  std::string resp = StaticDelete(kRoot, "tiny/to_delete.txt");
  REQUIRE(resp.find("HTTP/1.1 204") != std::string::npos);
  REQUIRE_FALSE(std::filesystem::exists("test_tree/tiny/to_delete.txt"));
}

TEST_CASE("StaticDelete 404 for missing file", "[Test_StaticFile]") {
  std::string resp = StaticDelete(kRoot, "tiny/ghost.txt");
  REQUIRE(resp.find("HTTP/1.1 404") != std::string::npos);
}

TEST_CASE("StaticDelete 403 for path traversal", "[Test_StaticFile]") {
  std::string resp = StaticDelete(kRoot, "../../etc/passwd");
  REQUIRE(resp.find("HTTP/1.1 403") != std::string::npos);
}
