#!/usr/bin/env python3
"""
Retrieval evaluation harness.

Measures MRR@10, Hits@1, and Hits@5 for BM25-only, semantic-only, and
hybrid search and prints a comparison table.

Usage:
    python3 eval/run_eval.py              # default localhost:8080
    python3 eval/run_eval.py --port 9090
    python3 eval/run_eval.py --queries path/to/queries.json
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any


def query_api(host: str, port: int, q: str) -> dict[str, Any]:
    url = (
        f"http://{host}:{port}/api/search"
        f"?q={urllib.parse.quote_plus(q)}"
    )
    with urllib.request.urlopen(url, timeout=15) as resp:
        return json.loads(resp.read())


def mrr_at_k(ranked: list[str], relevant: set[str], k: int) -> float:
    for i, doc_id in enumerate(ranked[:k]):
        if doc_id in relevant:
            return 1.0 / (i + 1)
    return 0.0


def hits_at_k(ranked: list[str], relevant: set[str], k: int) -> float:
    return 1.0 if any(d in relevant for d in ranked[:k]) else 0.0


def rank_list(result: dict[str, Any], method: str) -> list[str]:
    return [item["id"] for item in result.get(method, [])]


def run(queries: list[dict], host: str, port: int) -> None:
    methods = ["bm25", "semantic", "hybrid"]
    scores: dict[str, dict[str, list[float]]] = {
        m: {"mrr@10": [], "hits@1": [], "hits@5": []} for m in methods
    }
    # also track per-type breakdown
    types = sorted({q.get("type", "?") for q in queries})
    per_type: dict[str, dict[str, dict[str, list[float]]]] = {
        t: {m: {"mrr@10": [], "hits@1": [], "hits@5": []} for m in methods}
        for t in types
    }

    failed = 0
    print(f"Running {len(queries)} queries against {host}:{port}...\n")

    for idx, q in enumerate(queries, 1):
        query_text = q["query"]
        relevant = set(q["relevant"])
        qtype = q.get("type", "?")
        try:
            result = query_api(host, port, query_text)
        except urllib.error.URLError as exc:
            print(f"  [{idx:2d}] ERROR: {exc}")
            failed += 1
            continue

        status_parts = []
        for m in methods:
            ranked = rank_list(result, m)
            h1 = hits_at_k(ranked, relevant, 1)
            h5 = hits_at_k(ranked, relevant, 5)
            mrr = mrr_at_k(ranked, relevant, 10)
            scores[m]["mrr@10"].append(mrr)
            scores[m]["hits@1"].append(h1)
            scores[m]["hits@5"].append(h5)
            per_type[qtype][m]["mrr@10"].append(mrr)
            per_type[qtype][m]["hits@1"].append(h1)
            per_type[qtype][m]["hits@5"].append(h5)
            status_parts.append(f"{m[0].upper()}={int(h1)}")

        print(f"  [{idx:2d}] ({qtype:8s}) {query_text[:45]:<45}  {' '.join(status_parts)}")

    if failed == len(queries):
        print("\nAll queries failed. Is the server running?")
        sys.exit(1)

    n = len(queries) - failed

    def avg(vals: list[float]) -> float:
        return sum(vals) / len(vals) if vals else 0.0

    def row(label: str, m_scores: dict[str, dict[str, list[float]]]) -> str:
        cols = []
        for m in methods:
            cols.append(
                f"{avg(m_scores[m]['mrr@10']):>7.3f}"
                f"{avg(m_scores[m]['hits@1']):>7.3f}"
                f"{avg(m_scores[m]['hits@5']):>7.3f}"
            )
        return f"{label:<14}" + "".join(cols)

    header = (
        f"{'':14}"
        + "".join(
            f"{'-- ' + m.upper() + ' --':>21}" for m in methods
        )
    )
    subheader = (
        f"{'':14}"
        + "".join(f"{'MRR@10':>7}{'H@1':>7}{'H@5':>7}" for _ in methods)
    )
    divider = "-" * (14 + 21 * len(methods))

    print(f"\n{divider}")
    print(header)
    print(subheader)
    print(divider)
    print(row(f"ALL  (n={n})", scores))
    for t in types:
        t_scores = per_type[t]
        n_t = len(per_type[t][methods[0]]["mrr@10"])
        print(row(f"  {t:<12}", t_scores) + f"  n={n_t}")
    print(divider)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument(
        "--queries",
        default=str(Path(__file__).parent / "queries.json"),
        help="path to queries JSON file",
    )
    args = parser.parse_args()

    with open(args.queries) as f:
        queries = json.load(f)

    run(queries, args.host, args.port)


if __name__ == "__main__":
    main()
