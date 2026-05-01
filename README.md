# Semantic Search Engine

A hybrid full-text + semantic search engine built from scratch in C++ and Python, with a RAG-powered Q&A layer backed by Claude or Gemini.

## Overview

The system combines two complementary retrieval techniques:

- **BM25 keyword search** — a C++ inverted index ranks documents by term frequency, giving exact-match precision
- **Semantic search** — a Python microservice embeds queries and documents using `all-MiniLM-L6-v2` (384-dim), then retrieves by cosine similarity via FAISS
- **Hybrid ranking** — Reciprocal Rank Fusion (RRF, k=60) merges both result lists into a single ranked output that outperforms either method alone

On top of retrieval, a RAG Q&A endpoint passes the top-k results to an LLM (Claude Haiku or Gemini 2.0 Flash) and returns a grounded answer with source citations.

## Architecture

```
Browser / eval script
        │
        ▼
┌─────────────────────────────────┐
│  C++ HTTP Server  (port 8080)   │
│  - POSIX raw sockets            │
│  - Thread pool (N workers)      │
│  - Inverted index  (BM25)       │
│  - VectorClient  (HTTP client)  │
└────────────┬────────────────────┘
             │ HTTP (localhost:8001)
             ▼
┌─────────────────────────────────┐
│  Python Embed Service (FastAPI) │
│  - sentence-transformers        │
│  - FAISS IndexFlatIP            │
│  - Claude / Gemini  (RAG /ask)  │
└─────────────────────────────────┘
```

## Features

### Hybrid Search (BM25 + Semantic)
Queries hit both the inverted index and the FAISS vector index. Results are merged with Reciprocal Rank Fusion:

```
score(doc) = 1/(60 + rank_bm25) + 1/(60 + rank_semantic)
```

Documents that rank well in both lists get boosted; documents in only one list still receive a partial score.

### FAISS Index Persistence
On first startup the embed service walks `SEARCH_ROOT`, embeds all files in batches, and saves `embed_index.bin` + `embed_ids.json` to disk. Subsequent restarts load from cache in milliseconds instead of re-embedding.

### Live Index Updates
`PUT /static/<path>`, `POST /static/<path>`, and `DELETE /static/<path>` update both the BM25 inverted index and the FAISS vector index atomically in the same request.

### RAG Q&A
`GET /ask?q=<question>` retrieves the top-5 semantically relevant documents, passes their content to an LLM, and renders a grounded answer with clickable source links. Two providers are supported and switchable via an environment variable.

### Evaluation Harness
`eval/run_eval.py` benchmarks BM25, semantic, and hybrid search against 20 labeled queries (10 conceptual/semantic, 10 keyword-exact). Reports MRR@10, Hits@1, and Hits@5 per method with a per-query-type breakdown to show where each method wins.

## Quick Start

```bash
# 1. Install Python dependencies
pip install -r embed_service/requirements.txt

# 2. Set your LLM API key
export ANTHROPIC_API_KEY=sk-ant-...   # for Claude (default)
# or
export GOOGLE_API_KEY=AI...           # for Gemini

# 3. Build and run everything
cd searchserver
./start.sh          # builds C++ server, starts embed service, opens on :8080
```

Open [http://localhost:8080](http://localhost:8080) in your browser. Use the top form to search, the bottom form to ask a question.

## Configuration

| Environment variable | Default | Description |
|---|---|---|
| `PORT` | `8080` | C++ server port (first arg to `./start.sh`) |
| `SEARCH_ROOT` | `test_tree` | Directory to index |
| `LLM_PROVIDER` | `claude` | LLM for RAG: `claude` or `gemini` |
| `ANTHROPIC_API_KEY` | — | Required when `LLM_PROVIDER=claude` |
| `GOOGLE_API_KEY` | — | Required when `LLM_PROVIDER=gemini` |

```bash
# Use Gemini instead of Claude
LLM_PROVIDER=gemini ./start.sh

# Index a different directory on a different port
./start.sh 9090          # SEARCH_ROOT defaults to test_tree
SEARCH_ROOT=my_docs ./start.sh
```

## HTTP API

### Search

| Method | Path | Description |
|---|---|---|
| `GET` | `/` | Home page (search + ask forms) |
| `GET` | `/query?terms=<words>` | Hybrid search, returns HTML |
| `GET` | `/api/search?q=<words>` | Hybrid search, returns JSON |
| `GET` | `/ask?q=<question>` | RAG Q&A, returns HTML |
| `GET` | `/static/<path>` | Serve a file from `SEARCH_ROOT` |

### Index Mutation

| Method | Path | Description |
|---|---|---|
| `PUT` | `/static/<path>` | Create or replace a file (body = content) |
| `POST` | `/static/<path>` | Upload a new file (body = content) |
| `DELETE` | `/static/<path>` | Delete a file |

All mutation endpoints update both the BM25 inverted index and the FAISS vector index.

### Embed Service (port 8001)

| Method | Path | Description |
|---|---|---|
| `POST` | `/search` | `{"query": str, "k": int}` → TSV: `doc_id\tscore` per line |
| `POST` | `/ask` | `{"question": str, "k": int}` → `{answer}\n---SOURCES---\n{files}` |
| `POST` | `/index/add` | `{"id": str, "text": str}` → add/replace doc |
| `DELETE` | `/index/<path>` | Remove a doc from the vector index |
| `POST` | `/rebuild` | Rebuild the FAISS index from `SEARCH_ROOT` |
| `GET` | `/health` | `{"status": "ok", "indexed": N}` |

### `/api/search` JSON Response

```json
{
  "query": "white whale obsession",
  "bm25":     [{"id": "test_tree/books/mobydick.txt", "score": 84}, ...],
  "semantic": [{"id": "test_tree/books/mobydick.txt", "score": 0.712}, ...],
  "hybrid":   [{"id": "test_tree/books/mobydick.txt", "score": 0.031}, ...]
}
```

## Evaluation

```bash
# Server must be running
python3 eval/run_eval.py

# Custom port or query file
python3 eval/run_eval.py --port 9090
python3 eval/run_eval.py --queries path/to/my_queries.json
```

Sample output:

```
Running 20 queries against localhost:8080...

  [ 1] (semantic ) man wakes up transformed into a giant insect       B=0 S=1 H=1
  [ 2] (semantic ) deception speed and tactics in ancient warfare     B=1 S=1 H=1
  ...
  [11] (keyword  ) Sherlock Holmes Baker Street Watson mystery        B=1 S=0 H=1
  ...

--------------------------------------------------------------------
                  ------- BM25 -------  --- SEMANTIC ---  --- HYBRID ---
                  MRR@10   H@1   H@5   MRR@10   H@1   H@5   MRR@10   H@1   H@5
--------------------------------------------------------------------
ALL  (n=20)        ...      ...   ...    ...      ...   ...    ...      ...   ...
  keyword            ...                  ...                   ...          n=10
  semantic           ...                  ...                   ...          n=10
--------------------------------------------------------------------
```

The `queries.json` format is simple to extend:

```json
[
  {
    "query": "your natural language query",
    "relevant": ["test_tree/path/to/expected_doc.txt"],
    "type": "semantic"
  }
]
```

## Building

```bash
cd searchserver
make              # builds searchserver, searchclient, and test_suite
make searchserver # build only the server
./test_suite      # run unit tests (179 assertions)
make tidy-check   # run clang-tidy-19
make format       # run clang-format-19 (Chromium style)
make clean
```

Requires `clang++-19` and C++23. No third-party C++ libraries.

## Project Structure

```
.
├── embed_service/
│   ├── main.py              # FastAPI service: FAISS + sentence-transformers + LLM
│   └── requirements.txt
├── eval/
│   ├── queries.json         # 20 labeled test queries with ground-truth doc IDs
│   └── run_eval.py          # evaluation harness: MRR@10, Hits@1, Hits@5
└── searchserver/
    ├── HttpServer.cpp/hpp   # accept loop, thread dispatch, route handling
    ├── HttpRequest.cpp/hpp  # HTTP request parser
    ├── HttpResponse.cpp/hpp # HTTP response builder
    ├── InvertedIndex.cpp/hpp# BM25 inverted index
    ├── VectorClient.cpp/hpp # HTTP client for embed service
    ├── StaticFile.cpp/hpp   # static file GET/PUT/DELETE
    ├── ThreadPool.cpp/hpp   # fixed-size worker thread pool
    ├── searchserver.cpp     # main entry point
    ├── searchclient.cpp     # command-line HTTP client
    ├── sample_http/         # home page HTML template
    ├── test_tree/           # sample corpus (books, Enron emails, bash source)
    ├── Makefile
    └── start.sh             # one-command launcher
```
