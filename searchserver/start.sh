#!/usr/bin/env bash
# Starts both the embed service and the C++ search server.
# Usage: ./start.sh [port]   (default port: 8080)
set -euo pipefail

cd "$(dirname "$0")"   # always run from searchserver/

PORT=${1:-8080}
EMBED_PORT=8001

# make handles dependency tracking — recompiles only what changed, no-op if current
make searchserver

# Kill any leftover embed service from a previous run
if lsof -ti tcp:"$EMBED_PORT" >/dev/null 2>&1; then
    echo "==> Killing stale process on port $EMBED_PORT..."
    kill "$(lsof -ti tcp:"$EMBED_PORT")" 2>/dev/null || true
    sleep 1
fi

# Kill background children on exit (Ctrl+C or error)
cleanup() {
    echo ""
    echo "==> Shutting down..."
    kill "$EMBED_PID" 2>/dev/null || true
    wait "$EMBED_PID" 2>/dev/null || true
}
trap cleanup EXIT

echo "==> Starting embed service on port $EMBED_PORT..."
SEARCH_ROOT=test_tree python3 -m uvicorn embed_service.main:app \
    --app-dir .. --port "$EMBED_PORT" --host 127.0.0.1 2>&1 | \
    sed 's/^/[embed] /' &
EMBED_PID=$!

# Wait until the embed service is accepting connections (up to 30s)
echo "==> Waiting for embed service..."
for i in $(seq 1 30); do
    if curl -sf http://127.0.0.1:"$EMBED_PORT"/health >/dev/null 2>&1; then
        echo "==> Embed service ready."
        break
    fi
    if ! kill -0 "$EMBED_PID" 2>/dev/null; then
        echo "ERROR: embed service exited unexpectedly. Check output above."
        exit 1
    fi
    sleep 1
done

echo "==> Starting search server on port $PORT..."
echo "==> Open http://localhost:$PORT in your browser"
./searchserver "$PORT" test_tree
