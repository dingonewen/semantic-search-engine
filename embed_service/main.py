"""Embedding microservice: sentence-transformers + FAISS for semantic search.

Endpoints
---------
POST /search          {"query": str, "k": int} -> TSV: doc_id<TAB>score per line
POST /index/add       {"id": str, "text": str} -> {"status": "ok"}
DELETE /index/{path}  remove a document        -> {"status": "ok"}
POST /rebuild         rebuild from SEARCH_ROOT  -> {"status": "rebuilding"}
GET  /health          liveness check            -> {"status": "ok", "indexed": N}
"""

from __future__ import annotations

import os
import threading
from contextlib import asynccontextmanager

import faiss
import numpy as np
from fastapi import FastAPI
from fastapi.responses import PlainTextResponse
from pydantic import BaseModel
from sentence_transformers import SentenceTransformer

EMBED_DIM = 384  # all-MiniLM-L6-v2 output dimension
SEARCH_ROOT = os.environ.get("SEARCH_ROOT", "test_tree")
LLM_PROVIDER = os.environ.get("LLM_PROVIDER", "claude")  # "claude" or "gemini"
TEXT_LIMIT = 2048  # chars per document sent to the encoder
INDEX_BIN = "embed_index.bin"    # persisted FAISS index (CWD-relative)
IDS_JSON = "embed_ids.json"      # persisted doc ID list  (CWD-relative)

_model: SentenceTransformer | None = None
# Parallel arrays: _vecs[i] is the embedding for _ids[i]
_vecs: np.ndarray = np.zeros((0, EMBED_DIM), dtype=np.float32)
_ids: list[str] = []
_index: faiss.IndexFlatIP | None = None
_lock = threading.Lock()


def _make_index() -> faiss.IndexFlatIP:
    return faiss.IndexFlatIP(EMBED_DIM)


def _save_index() -> None:
    """Persist the current index and ID list to disk."""
    import json
    faiss.write_index(_index, INDEX_BIN)
    with open(IDS_JSON, "w") as f:
        json.dump(_ids, f)


def _try_load_index() -> bool:
    """Load a previously saved index. Returns True on success."""
    import json
    global _vecs, _ids, _index
    if not (os.path.exists(INDEX_BIN) and os.path.exists(IDS_JSON)):
        return False
    try:
        loaded = faiss.read_index(INDEX_BIN)
        with open(IDS_JSON) as f:
            ids = json.load(f)
        if loaded.ntotal != len(ids):
            return False
        # Reconstruct the parallel numpy array for add/remove operations
        vecs = np.zeros((loaded.ntotal, EMBED_DIM), dtype=np.float32)
        for i in range(loaded.ntotal):
            vecs[i] = loaded.reconstruct(i)
        with _lock:
            _index = loaded
            _ids = ids
            _vecs = vecs
        print(f"[embed_service] loaded {len(ids)} vectors from disk")
        return True
    except Exception as exc:  # noqa: BLE001
        print(f"[embed_service] cache load failed ({exc}), will rebuild")
        return False


@asynccontextmanager
async def lifespan(app: FastAPI):
    global _model, _index
    _model = SentenceTransformer("all-MiniLM-L6-v2")
    _index = _make_index()
    if not _try_load_index():
        # No cached index — build from scratch in the background
        threading.Thread(target=_rebuild_bg, args=(SEARCH_ROOT,), daemon=True).start()
    yield


app = FastAPI(lifespan=lifespan)


# ── helpers ──────────────────────────────────────────────────────────────────

def _encode(text: str) -> np.ndarray:
    vec = _model.encode([text[:TEXT_LIMIT]], normalize_embeddings=True)
    return vec.astype(np.float32)


def _rebuild_bg(root: str) -> None:
    """Walk root, embed every file, rebuild the in-memory FAISS index."""
    from pathlib import Path

    files = [p for p in Path(root).rglob("*") if p.is_file()]
    if not files:
        return

    texts, ids = [], []
    for fpath in files:
        try:
            texts.append(fpath.read_text(errors="replace")[:TEXT_LIMIT])
            ids.append(str(fpath))
        except OSError:
            pass

    vecs = _model.encode(
        texts, normalize_embeddings=True, batch_size=32, show_progress_bar=True
    ).astype(np.float32)

    new_index = _make_index()
    new_index.add(vecs)

    global _vecs, _ids, _index
    with _lock:
        _vecs = vecs
        _ids = list(ids)
        _index = new_index

    print(f"[embed_service] indexed {len(ids)} documents from '{root}'")
    _save_index()
    print(f"[embed_service] index saved to {INDEX_BIN}")


# ── routes ───────────────────────────────────────────────────────────────────

class SearchReq(BaseModel):
    query: str
    k: int = 10


class AddReq(BaseModel):
    id: str
    text: str


@app.post("/search", response_class=PlainTextResponse)
def search(req: SearchReq) -> str:
    """Return TSV lines: doc_id<TAB>score (cosine similarity, descending)."""
    with _lock:
        if _index is None or _index.ntotal == 0:
            return ""
        q = _encode(req.query)
        k = min(req.k, _index.ntotal)
        scores, idxs = _index.search(q, k)

    lines: list[str] = []
    for score, idx in zip(scores[0], idxs[0]):
        if 0 <= idx < len(_ids):
            lines.append(f"{_ids[idx]}\t{float(score):.6f}")
    return "\n".join(lines)


@app.post("/index/add")
def add_doc(req: AddReq):
    """Embed and insert (or replace) a document."""
    global _vecs, _ids, _index

    vec = _encode(req.text)
    new_index = _make_index()

    with _lock:
        if req.id in _ids:
            # Drop existing entry and rebuild without it
            keep = [i for i, did in enumerate(_ids) if did != req.id]
            kept_vecs = _vecs[keep] if keep else np.zeros((0, EMBED_DIM), dtype=np.float32)
            kept_ids = [_ids[i] for i in keep]
        else:
            kept_vecs = _vecs
            kept_ids = list(_ids)

        _vecs = np.vstack([kept_vecs, vec]) if kept_vecs.shape[0] > 0 else vec
        _ids = kept_ids + [req.id]
        new_index.add(_vecs)
        _index = new_index

    return {"status": "ok"}


@app.delete("/index/{doc_id:path}")
def remove_doc(doc_id: str):
    """Remove a document from the vector index."""
    global _vecs, _ids, _index

    with _lock:
        if doc_id not in _ids:
            return {"status": "not_found"}
        keep = [i for i, did in enumerate(_ids) if did != doc_id]
        _vecs = _vecs[keep] if keep else np.zeros((0, EMBED_DIM), dtype=np.float32)
        _ids = [_ids[i] for i in keep]
        new_index = _make_index()
        if _vecs.shape[0] > 0:
            new_index.add(_vecs)
        _index = new_index

    return {"status": "ok"}


_SYSTEM_PROMPT = (
    "You are a helpful assistant. Answer the user's question based only on "
    "the provided documents. Cite sources by their filename in brackets."
)


def _llm_answer(question: str, context: str) -> str:
    """Call the configured LLM provider and return the answer text."""
    if LLM_PROVIDER == "gemini":
        import google.generativeai as genai
        genai.configure(api_key=os.environ["GOOGLE_API_KEY"])
        model = genai.GenerativeModel(
            "gemini-2.0-flash", system_instruction=_SYSTEM_PROMPT
        )
        return model.generate_content(
            f"Documents:\n\n{context}\n\nQuestion: {question}"
        ).text
    else:
        import anthropic
        client = anthropic.Anthropic()
        msg = client.messages.create(
            model="claude-haiku-4-5-20251001",
            max_tokens=1024,
            system=[{"type": "text", "text": _SYSTEM_PROMPT,
                      "cache_control": {"type": "ephemeral"}}],
            messages=[{"role": "user",
                       "content": f"Documents:\n\n{context}\n\nQuestion: {question}"}],
        )
        return msg.content[0].text


class AskReq(BaseModel):
    question: str
    k: int = 5


@app.post("/ask", response_class=PlainTextResponse)
def ask(req: AskReq) -> str:
    """Retrieve top-k docs semantically, call Claude, return answer + sources.

    Response format: {answer text}\n---SOURCES---\n{file1}\n{file2}\n...
    """
    from pathlib import Path

    with _lock:
        if _index is None or _index.ntotal == 0:
            return "No documents indexed yet.\n---SOURCES---\n"
        q = _encode(req.question)
        k = min(req.k, _index.ntotal)
        scores, idxs = _index.search(q, k)
        ids_snapshot = list(_ids)

    sources: list[str] = []
    context_parts: list[str] = []
    for score, idx in zip(scores[0], idxs[0]):
        if 0 <= idx < len(ids_snapshot):
            doc_id = ids_snapshot[idx]
            sources.append(doc_id)
            try:
                text = Path(doc_id).read_text(errors="replace")[:TEXT_LIMIT]
            except OSError:
                text = "(unreadable)"
            context_parts.append(f"[{doc_id}]\n{text}")

    context = "\n\n---\n\n".join(context_parts)

    answer = _llm_answer(req.question, context)
    return f"{answer}\n---SOURCES---\n" + "\n".join(sources)


@app.post("/rebuild")
def rebuild(root: str = SEARCH_ROOT):
    """Trigger a background rebuild from the given directory."""
    threading.Thread(target=_rebuild_bg, args=(root,), daemon=True).start()
    return {"status": "rebuilding", "root": root}


@app.get("/health")
def health():
    indexed = _index.ntotal if _index is not None else 0
    return {"status": "ok", "indexed": indexed}


@app.get("/", response_class=PlainTextResponse)
def root():
    indexed = _index.ntotal if _index is not None else 0
    return (
        "Embed service running.\n"
        f"  indexed documents : {indexed}\n"
        f"  model             : all-MiniLM-L6-v2\n"
        f"  search endpoint   : POST /search\n"
        f"  health endpoint   : GET  /health\n"
    )
