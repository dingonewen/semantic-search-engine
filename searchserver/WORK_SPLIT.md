# Work split and file ownership

Overview
--------
This file maps each header/source to a partner to avoid merge conflicts. Each partner should work in their assigned files and avoid editing the other partner's files unless coordinating.

Yiwen
- Ownership: network, connection handling, HTTP serialization
- Files to implement (headers created):
  - `SocketWrapper.hpp` / SocketWrapper.cpp
  - `ClientConnection.hpp` / ClientConnection.cpp
  - `HttpRequest.hpp` / HttpRequest.cpp (parsing helpers)
  - `HttpResponse.hpp` / HttpResponse.cpp (to_string serialization)
  - `Server.hpp` / Server.cpp (accept loop, connection logging)
  - ENSURE there is at least one test for every function

Responsibilities:
- Implement safe socket RAII and wrapper helpers.
- Accept connections, produce `ClientConnection` objects.
- Parse raw bytes into `HttpRequest` and serialize `HttpResponse`.
- Handle Connection: close header, and graceful shutdown signaling.

JC
- Ownership: indexing, crawling, search logic, client shell
- Files to implement (headers created):
  - `FileCrawler.hpp` / FileCrawler.cpp
  - `Index.hpp` / Index.cpp
  - `SearchEngine.hpp` / SearchEngine.cpp
  - `ClientShell.hpp` / ClientShell.cpp
  - `Utils.hpp` / Utils.cpp (tokenization, lowercase)
  - ENSURE there is at least one test for every function

Responsibilities:
- Crawl `test_tree` and other directories to collect file contents.
- Build the inverted index and implement `search()` ranking.
- Provide client shell to exercise get/post/put/delete.
- Provide tokenization and normalization utilities.

Shared / Coordination
- `ThreadPool.hpp` is provided in the repo; confirm implementation. If modifications are required, coordinate via a short PR or communicate.
- `Utils.hpp` is implemented by PartnerB; PartnerA may add thin helpers but should avoid editing `Utils.hpp` without coordination.
- API boundaries: Server and ClientConnection use SearchEngine and ThreadPool. PartnerA should call `SearchEngine::search(...)` and `SearchEngine::build_index()` but should not modify `SearchEngine` internals.

Notes
- Keep changes small and frequent; create feature branches and open PRs rather than pushing directly to `main`.
- When adding .cpp files, add them to the `Makefile` and notify partner to avoid Makefile edit conflicts (one person should own the Makefile edits — agree who).

