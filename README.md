<div align="center">
  <img src="https://capsule-render.vercel.app/api?type=waving&color=0:282c34,100:61afef&height=220&section=header&text=Dingle&fontSize=80&fontColor=abb2bf&animation=fadeIn&fontAlignY=40" />
</div>

<p align="center">
  <strong>"Your documents. Your knowledge. Search it like a brain."</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B23-HTTP%20Server-e06c75?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++23"/>
  <img src="https://img.shields.io/badge/FAISS-Vector%20Search-61afef?style=for-the-badge&logo=meta&logoColor=white" alt="FAISS"/>
  <img src="https://img.shields.io/badge/Anthropic%20Claude-RAG-c678dd?style=for-the-badge&logo=anthropic&logoColor=white" alt="Claude"/>
  <img src="https://img.shields.io/badge/FastAPI-Python-98c379?style=for-the-badge&logo=fastapi&logoColor=white" alt="FastAPI"/>
</p>

<p align="center">
  <em>A full-stack AI search engine built from scratch — BM25 keyword search fused with semantic vector search, topped with a RAG Q&A layer. Index your own documents and query them in natural language.</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/BM25-Keyword%20Search-e5c07b?style=for-the-badge" alt="BM25"/>
  <img src="https://img.shields.io/badge/RRF-Hybrid%20Ranking-56b6c2?style=for-the-badge" alt="RRF"/>
  <img src="https://img.shields.io/badge/sentence--transformers-Embeddings-d19a66?style=for-the-badge" alt="sentence-transformers"/>
</p>

<p align="center">
  <img src="assets/Screenshot 2026-05-02 122125.png" alt="Dingle UI Screenshot" width="800"/>
</p>

---

---

## Tech Stack

| Layer | Technology |
|---|---|
| HTTP Server | C++23, POSIX raw sockets, custom thread pool |
| Keyword Search | BM25 inverted index (built from scratch in C++) |
| Semantic Search | `all-MiniLM-L6-v2` (384-dim) + FAISS `IndexFlatIP` |
| Hybrid Ranking | Reciprocal Rank Fusion (RRF, k=60) |
| RAG / LLM | Anthropic Claude Haiku · Google Gemini 2.0 Flash |
| Embed Service | Python · FastAPI · sentence-transformers |
| UI | Vanilla JS SPA · Atom One Dark · Fontdiner Swanky · Space Grotesk |
| Config | `python-dotenv` · environment variables |

---

## Architecture

```
Browser
   │
   ▼
┌──────────────────────────────────────┐
│   C++ HTTP Server  (port 8080)       │
│                                      │
│   ┌─────────────┐  ┌──────────────┐  │
│   │ BM25 Index  │  │ VectorClient │  │
│   │ (InvertedIn │  │ (HTTP to     │  │
│   │  vertedIndex│  │  port 8001)  │  │
│   └──────┬──────┘  └──────┬───────┘  │
│          └────────┬────────┘          │
│              RRF Merge                │
│         /api/search  /api/ask         │
└──────────────────────────────────────┘
                    │ localhost:8001
                    ▼
┌──────────────────────────────────────┐
│   Python Embed Service  (FastAPI)    │
│                                      │
│   sentence-transformers (384-dim)    │
│   FAISS IndexFlatIP                  │
│   Claude Haiku / Gemini 2.0 Flash    │
└──────────────────────────────────────┘
```

---

## How It Works

### 1. Hybrid Search (BM25 + Semantic)

Every query runs through **two independent retrieval pipelines in parallel**:

- **BM25** — the C++ inverted index scores documents by term frequency and inverse document frequency, giving strong exact-match precision
- **Semantic** — the Python service encodes the query into a 384-dim vector and retrieves nearest neighbors from the FAISS index by cosine similarity

Results are merged with **Reciprocal Rank Fusion**:

```
score(doc) = 1 / (60 + rank_bm25) + 1 / (60 + rank_semantic)
```

Documents that rank well in both lists get boosted. Documents that appear in only one list still receive a partial score. This consistently outperforms either method alone.

A **cosine similarity threshold** (≥ 0.3) filters out semantically irrelevant documents before RRF, preventing noise from polluting the hybrid ranking.

### 2. RAG Q&A Pipeline

`/api/ask` retrieves the top-k semantically relevant documents, assembles them as context, and calls an LLM to synthesize a grounded answer with source citations. The system prompt is cached with Anthropic's prompt caching to reduce latency and cost on repeated calls.

### 3. FAISS Index Persistence

On first startup, the embed service walks `SEARCH_ROOT`, embeds all documents in batches, and persists `embed_index.bin` + `embed_ids.json` to disk. Subsequent restarts load from cache in milliseconds.

### 4. Live Index Updates

`PUT`, `POST`, and `DELETE` on `/static/<path>` update both the BM25 inverted index and the FAISS vector index atomically in a single request — no restart required.

---

## UI — Dingle

A single-page app with no framework dependencies.

- **Evidence column** — search results as ranked cards (filename, relevance bar, text snippet). Click any card to open the source document.
- **Synthesis column** — the LLM's RAG answer with cited sources, streamed after "Ask My Brain" is submitted.
- Submitting a new search clears the synthesis panel automatically.

---

## Quick Start

```bash
# 1. Install Python dependencies
pip install -r embed_service/requirements.txt

# 2. Configure API keys
cp .env.example .env
# Edit .env and set ANTHROPIC_API_KEY (default) or GOOGLE_API_KEY

# 3. Add your documents
mkdir -p search_content
# Copy your .txt files into search_content/ (subdirectories supported)

# 4. Build and launch
cd searchserver
./start.sh
```

Open [http://localhost:8080](http://localhost:8080).

The first launch embeds all documents and saves the index to disk. Subsequent starts are instant.

---

## Configuration

Copy `.env.example` to `.env` and fill in your keys. Never commit `.env`.

| Variable | Default | Description |
|---|---|---|
| `ANTHROPIC_API_KEY` | — | Required when `LLM_PROVIDER=claude` |
| `GOOGLE_API_KEY` | — | Required when `LLM_PROVIDER=gemini` |
| `LLM_PROVIDER` | `claude` | `claude` or `gemini` |
| `SEARCH_ROOT` | `search_content` | Directory to index (relative to `searchserver/`) |

```bash
# Switch to Gemini
LLM_PROVIDER=gemini ./start.sh

# Index a different directory
SEARCH_ROOT=my_docs ./start.sh

# Custom port
./start.sh 9090
```

---

## API Reference

### C++ Server (port 8080)

| Method | Path | Description |
|---|---|---|
| `GET` | `/` | Home page (SPA) |
| `GET` | `/api/search?terms=<words>` | Hybrid search — returns JSON |
| `GET` | `/api/ask?q=<question>` | RAG Q&A — returns JSON |
| `GET` | `/static/<path>` | Serve a raw file from `SEARCH_ROOT` |
| `PUT` | `/static/<path>` | Create or replace a file (body = content) |
| `POST` | `/static/<path>` | Upload a new file |
| `DELETE` | `/static/<path>` | Delete a file |

#### `/api/search` response

```json
{
  "query": "machine learning projects",
  "bm25":     [{ "id": "search_content/projects.txt", "score": 84 }],
  "semantic": [{ "id": "search_content/projects.txt", "score": 0.712 }],
  "hybrid":   [{ "id": "search_content/projects.txt", "score": 0.031 }]
}
```

#### `/api/ask` response

```json
{
  "answer": "Based on the provided documents...",
  "sources": ["search_content/projects.txt", "search_content/resume.txt"]
}
```

### Python Embed Service (port 8001)

| Method | Path | Description |
|---|---|---|
| `POST` | `/search` | `{"query": str, "k": int}` → TSV: `doc_id\tscore` |
| `POST` | `/ask` | `{"question": str, "k": int}` → answer + sources |
| `POST` | `/index/add` | `{"id": str, "text": str}` → add/replace doc |
| `DELETE` | `/index/<path>` | Remove a doc from the vector index |
| `POST` | `/rebuild` | Rebuild FAISS index from `SEARCH_ROOT` |
| `GET` | `/health` | `{"status": "ok", "indexed": N}` |

---

## Evaluation

The eval harness benchmarks BM25, semantic, and hybrid search against labeled queries and reports **MRR@10**, **Hits@1**, and **Hits@5** per method with a per-query-type breakdown.

```bash
# Server must be running
python3 eval/run_eval.py

# Custom port or query file
python3 eval/run_eval.py --port 9090
python3 eval/run_eval.py --queries eval/queries.json
```

Query file format:

```json
[
  {
    "query": "your natural language query",
    "relevant": ["search_content/path/to/expected_doc.txt"],
    "type": "semantic"
  }
]
```

---

## Building the C++ Server

```bash
cd searchserver
make              # builds searchserver, searchclient, test_suite
make searchserver # server only
./test_suite      # unit tests (179 assertions)
make tidy-check   # clang-tidy-19
make format       # clang-format-19 (Chromium style)
make clean
```

Requires `clang++-19` and C++23. No third-party C++ libraries — HTTP parsing, thread pool, and inverted index are all implemented from scratch.

---

## Project Structure

```
.
├── .env.example             # API key template — copy to .env
├── embed_service/
│   ├── main.py              # FastAPI: FAISS + sentence-transformers + LLM RAG
│   └── requirements.txt
├── eval/
│   ├── queries.json         # labeled test queries with ground-truth doc IDs
│   └── run_eval.py          # evaluation harness: MRR@10, Hits@1, Hits@5
├── search_content/          # your documents go here (.txt, subdirs supported)
└── searchserver/
    ├── HttpServer.cpp/hpp   # request routing, RRF merge, RAG orchestration
    ├── HttpRequest.cpp/hpp  # HTTP/1.1 request parser
    ├── HttpResponse.cpp/hpp # HTTP response builder
    ├── InvertedIndex.cpp/hpp# BM25 inverted index
    ├── VectorClient.cpp/hpp # HTTP client for the embed service
    ├── StaticFile.cpp/hpp   # file GET / PUT / DELETE with live index update
    ├── ThreadPool.cpp/hpp   # fixed-size worker thread pool
    ├── searchserver.cpp     # main entry point
    ├── searchclient.cpp     # CLI HTTP client for testing
    ├── sample_http/         # Dingle SPA (HTML/CSS/JS)
    ├── start.sh             # one-command build + launch
    └── Makefile
```
