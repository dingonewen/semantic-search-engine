#pragma once
#include <string>
#include <utility>
#include <vector>

namespace searchserver {

// Non-owning HTTP client for the embedding microservice running on localhost.
// Every method returns empty results / silently ignores errors when the
// service is unreachable, so BM25-only search continues to work without it.
class VectorClient {
 public:
  // port: TCP port the embed service listens on (default 8001).
  explicit VectorClient(int port = 8001);

  // Query the vector index. Returns (doc_id, cosine_score) pairs ranked by
  // score descending. Returns {} if the service is unavailable.
  auto Search(const std::string& query, int k = 10) const
      -> std::vector<std::pair<std::string, float>>;

  // Add or replace a document in the vector index (fire-and-forget).
  void AddDoc(const std::string& doc_id, const std::string& text) const;

  // Remove a document from the vector index (fire-and-forget).
  void RemoveDoc(const std::string& doc_id) const;

  // Ask a natural-language question using RAG.
  // Retrieves top-k docs semantically, passes them to Claude, and returns
  // (answer, source_file_list). Returns ("", {}) if the service is down.
  auto Ask(const std::string& question, int k = 5) const
      -> std::pair<std::string, std::vector<std::string>>;

 private:
  int m_port;

  // Opens a TCP connection to localhost:m_port. Returns fd >= 0 on success,
  // -1 on failure.
  auto Connect() const -> int;

  // Sends an HTTP POST to path with a JSON body. Returns the response body.
  auto Post(const std::string& path, const std::string& json_body) const
      -> std::string;

  // Sends an HTTP DELETE to path. Returns the response body.
  auto Delete(const std::string& path) const -> std::string;

  // Parses the TSV response from /search into (doc_id, score) pairs.
  static auto ParseTsv(const std::string& body)
      -> std::vector<std::pair<std::string, float>>;

  // Reads an HTTP response from fd and returns the body.
  static auto ReadResponse(int fd) -> std::string;

  // Escapes a string for embedding inside a JSON string literal.
  static auto JsonEscape(const std::string& s) -> std::string;
};

}  // namespace searchserver
