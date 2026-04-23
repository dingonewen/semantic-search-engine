#include "InvertedIndex.hpp"
#include "catch.hpp"

#include <algorithm>
#include <string>
#include <vector>

// Helper: find the score for a given filename in results, returns -1 if absent
static int find_score(const std::vector<std::pair<std::string, int>>& results,
                      const std::string& filename) {
  for (auto& p : results) {
    // match on the last path component or full path suffix
    if (p.first == filename ||
        (p.first.size() >= filename.size() &&
         p.first.substr(p.first.size() - filename.size()) == filename))
      return p.second;
  }
  return -1;
}

// Helper: get rank (0-based) of a filename in sorted results, -1 if absent
static int find_rank(const std::vector<std::pair<std::string, int>>& results,
                     const std::string& filename) {
  for (int i = 0; i < static_cast<int>(results.size()); ++i) {
    if (results[i].first == filename ||
        (results[i].first.size() >= filename.size() &&
         results[i].first.substr(results[i].first.size() - filename.size()) ==
             filename))
      return i;
  }
  return -1;
}

// ─── Tokenize ────────────────────────────────────────────────────────────────

TEST_CASE("Tokenize splits on non-alphanumeric characters",
          "[Test_InvertedIndex]") {
  auto toks = InvertedIndex::Tokenize("hello, world! foo-bar");
  REQUIRE(toks.size() == 4);
  REQUIRE(toks[0] == "hello");
  REQUIRE(toks[1] == "world");
  REQUIRE(toks[2] == "foo");
  REQUIRE(toks[3] == "bar");
}

TEST_CASE("Tokenize lowercases all tokens", "[Test_InvertedIndex]") {
  auto toks = InvertedIndex::Tokenize("Buffalo BUFFALO buffalo");
  REQUIRE(toks.size() == 3);
  for (auto& t : toks)
    REQUIRE(t == "buffalo");
}

TEST_CASE("Tokenize returns empty vector for empty string",
          "[Test_InvertedIndex]") {
  REQUIRE(InvertedIndex::Tokenize("").empty());
}

TEST_CASE("Tokenize returns empty vector for all punctuation",
          "[invertedindex]") {
  REQUIRE(InvertedIndex::Tokenize("!!! --- ...").empty());
}

// ─── Build + SearchAndRank ─────────────────────────────────────────────────

TEST_CASE("search 'buffalo' matches expected files and scores",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");

  auto results = idx.SearchAndRank({"buffalo"});

  // Shows 9 results
  REQUIRE(results.size() == 9);

  // Results must be sorted in descending order of score
  for (size_t i = 1; i < results.size(); ++i)
    REQUIRE(results[i - 1].second >= results[i].second);

  // Verify top result and score from screenshot
  REQUIRE(find_rank(results, "books/mobydick.txt") == 0);
  REQUIRE(find_score(results, "books/mobydick.txt") == 10);

  // Second result
  REQUIRE(find_rank(results, "tiny/buffalo.txt") == 1);
  REQUIRE(find_score(results, "tiny/buffalo.txt") == 8);

  // Third result
  REQUIRE(find_score(results, "books/thejunglebook.txt") == 7);

  // Check a few more scores
  REQUIRE(find_score(results, "books/leavesofgrass.txt") == 3);
  REQUIRE(find_score(results, "books/ulysses.txt") == 2);
  REQUIRE(find_score(results, "tiny/home-on-the-range.txt") == 1);
  REQUIRE(find_score(results, "books/tomsawyer.txt") == 1);
  REQUIRE(find_score(results, "books/sherlockholmes.txt") == 1);
  REQUIRE(find_score(results, "books/davincinotebooks.txt") == 1);
}

TEST_CASE("search 'Buffalo' (mixed case) gives same results as 'buffalo'",
          "[Test_InvertedIndex]") {
  // split_terms() lowercases the query before passing to SearchAndRank.
  // Verify that split_terms lowercases "Buffalo" -> "buffalo" so the caller
  // gets the same results regardless of input case.
  auto lower_terms = InvertedIndex::Tokenize("buffalo");
  auto upper_terms = InvertedIndex::Tokenize("Buffalo");

  // Tokenize must lowercase both to the same token
  REQUIRE(lower_terms == upper_terms);
  REQUIRE(lower_terms.size() == 1);
  REQUIRE(lower_terms[0] == "buffalo");

  // Searching with the lowercased terms must give the same results
  InvertedIndex idx;
  idx.Build("test_tree");
  auto lower = idx.SearchAndRank(lower_terms);
  auto upper = idx.SearchAndRank(upper_terms);
  REQUIRE(lower.size() == upper.size());
  for (size_t i = 0; i < lower.size(); ++i) {
    REQUIRE(lower[i].first == upper[i].first);
    REQUIRE(lower[i].second == upper[i].second);
  }
}

TEST_CASE("search 'hat magic' returns same results as 'magic hat'",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");

  auto hat_magic = idx.SearchAndRank({"hat", "magic"});
  auto magic_hat = idx.SearchAndRank({"magic", "hat"});

  // Same number of results regardless of term order
  REQUIRE(hat_magic.size() == magic_hat.size());

  // Sort both by (score DESC, filename ASC) before comparing to eliminate
  // non-deterministic ordering of tied-score entries
  auto stable_sort = [](std::vector<std::pair<std::string, int>>& v) {
    std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
      return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
  };
  stable_sort(hat_magic);
  stable_sort(magic_hat);

  for (size_t i = 0; i < hat_magic.size(); ++i) {
    REQUIRE(hat_magic[i].first == magic_hat[i].first);
    REQUIRE(hat_magic[i].second == magic_hat[i].second);
  }
}

TEST_CASE("search 'hat magic' matches expected files and scores",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");

  auto results = idx.SearchAndRank({"hat", "magic"});

  // Results must be sorted in descending order of score
  for (size_t i = 1; i < results.size(); ++i)
    REQUIRE(results[i - 1].second >= results[i].second);

  // Top result must be ulysses with score 173
  REQUIRE(find_rank(results, "books/ulysses.txt") == 0);
  REQUIRE(find_score(results, "books/ulysses.txt") == 173);

  // Verify several scores
  REQUIRE(find_score(results, "books/lesmiserables.txt") == 99);
  REQUIRE(find_score(results, "books/countofmontecristo.txt") == 63);
  REQUIRE(find_score(results, "books/warandpeace.txt") == 54);
  REQUIRE(find_score(results, "books/mobydick.txt") == 49);
  REQUIRE(find_score(results, "books/huckfinn.txt") == 29);
  REQUIRE(find_score(results, "books/artofwar.txt") == 2);
}

// ─── AddFile / RemoveFile ──────────────────────────────────────────────────

TEST_CASE("RemoveFile removes a file's entries from the index",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");

  // Before removal, buffalo.txt should appear in buffalo search
  auto before = idx.SearchAndRank({"buffalo"});
  REQUIRE(find_score(before, "tiny/buffalo.txt") > 0);

  // Remove it
  idx.RemoveFile("test_tree/tiny/buffalo.txt");

  auto after = idx.SearchAndRank({"buffalo"});
  REQUIRE(find_score(after, "tiny/buffalo.txt") == -1);
  // One fewer result
  REQUIRE(after.size() == before.size() - 1);
}

TEST_CASE("AddFile re-indexes a previously removed file",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");

  int original_score =
      find_score(idx.SearchAndRank({"buffalo"}), "tiny/buffalo.txt");
  REQUIRE(original_score > 0);

  idx.RemoveFile("test_tree/tiny/buffalo.txt");
  REQUIRE(find_score(idx.SearchAndRank({"buffalo"}), "tiny/buffalo.txt") == -1);

  idx.AddFile("test_tree/tiny/buffalo.txt");
  int restored_score =
      find_score(idx.SearchAndRank({"buffalo"}), "tiny/buffalo.txt");
  REQUIRE(restored_score == original_score);
}

TEST_CASE("search for unknown term returns empty results",
          "[Test_InvertedIndex]") {
  InvertedIndex idx;
  idx.Build("test_tree");
  auto results = idx.SearchAndRank({"zzzznotaword99999"});
  REQUIRE(results.empty());
}
