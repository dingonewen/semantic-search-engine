# Collaboration Guide — Search Server

This document defines how to split work between two people to minimize merge conflicts and enable parallel development.

---

## Project Overview

```
searchserver/
├── HttpRequest.cpp/hpp       # HTTP request parsing
├── HttpResponse.cpp/hpp      # HTTP response building
├── HttpServer.cpp/hpp        # Core server loop + request dispatch
├── InvertedIndex.cpp/hpp     # Index build, file add/remove, search & rank
├── StaticFile.cpp/hpp        # Static file serving
├── ThreadPool.cpp/hpp        # Thread pool (worker threads + task queue)
├── searchserver.cpp          # main() — server entry point
├── searchclient.cpp          # Client CLI — sends HTTP queries
├── test_threadpool.cpp       # Unit tests for ThreadPool
└── test_suite.cpp            # Integration test harness
```

---

## Split by Person

### Yiwen — Server & Concurrency

**Owns these files (do not edit without coordinating with Yiwen):**

| File | Role |
|------|------|
| `HttpServer.cpp/hpp` ✅| Accepts connections, dispatches requests to ThreadPool |
| `HttpRequest.cpp/hpp` ✅| Parses raw HTTP into `Request` struct; `split_terms()` |
| `HttpResponse.cpp/hpp` ✅| Builds HTTP response strings via `make_response()` |
| `ThreadPool.cpp/hpp` ✅| Worker threads, task queue, mutex/CV synchronization |
| `searchserver.cpp` ✅| `main()` — wires everything together, parses CLI args |
| `test_threadpool.cpp` ✅| Tests for ThreadPool |

**Yiwen's responsibilities:**
- Keep the server accepting concurrent connections correctly
- Ensure `ThreadPool` passes its tests (`./test_suite`)
- Define and freeze the `Request` struct and `make_response()` signature **early** so JC can depend on them

---

### JC — Search & Content

**Owns these files (do not edit without coordinating with JC):**

| File | Role |
|------|------|
| `InvertedIndex.cpp/hpp` | `build()`, `add_file()`, `remove_file()`, `search_and_rank()` |
| `StaticFile.cpp/hpp` | Serves static files from disk, uses `make_response()` |
| `searchclient.cpp` | CLI client — URL-encodes queries, sends HTTP GET, prints results |
| `test_suite.cpp` | End-to-end and search correctness tests |

**JC's responsibilities:**
- Build and test `InvertedIndex` independently using its own unit tests
- Implement `StaticFile` using the `make_response()` interface agreed with Yiwen
- Keep `searchclient` working against the running server

---

## Shared Interface Contract (agree on this first✅)

These are the **only touch points** between the two halves. Lock them down before coding:

### 1. `Request` struct (`HttpRequest.hpp`)
```cpp
struct Request {
  std::string method;   // "GET", "POST", etc.
  std::string path;     // e.g. "/static/foo.html"
  std::string query;    // e.g. "term1+term2"
  std::unordered_map<std::string, std::string> headers;
};
```

### 2. `make_response()` (`HttpResponse.hpp`)
```cpp
std::string make_response(int status,
                          const std::string& body,
                          const std::string& content_type = "text/plain",
                          const std::string& status_text = "");
```

### 3. `InvertedIndex::search_and_rank()` (`InvertedIndex.hpp`)
```cpp
std::vector<std::pair<std::string, int>>
    search_and_rank(const std::vector<std::string>& terms) const;
```
`HttpServer` calls this — Yiwen calls into JC's code here. **Do not change the signature without notifying both sides.**

---

## Git Workflow to Avoid Merge Conflicts

1. **Branch per person:**
   ```
   git checkout -b person-a/server-concurrency
   git checkout -b person-b/search-content
   ```

2. **Never edit the other person's files** unless you open a PR/discussion first.

3. **Shared files** (`Makefile`, `test_suite.cpp`) — coordinate before editing:
   - For `Makefile`: Yiwen adds server-side build rules; JC adds search-side rules. Edit in separate commits and merge carefully.
   - For `test_suite.cpp`: each person adds their own `TEST_CASE` blocks only.

4. **Sync frequently** — rebase onto `main` at least once a day:
   ```
   git fetch origin
   git rebase origin/main
   ```

5. **Integration milestone** — once both halves compile independently, do a joint merge into `main` and run:
   ```
   make
   ./searchserver 5950 test_tree &
   ./searchclient :: 5950
   ```

---

## Build & Test

```bash
# Build everything
make

# Run server
./searchserver <port> <index_root>

# Run client
./searchclient :: <port>

# Run unit tests
./test_suite
```
