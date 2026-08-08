#!/usr/bin/env bash
# Local preview of downloads.html over HTTP (not file:// — videos and relative paths).
# On Windows PowerShell do not use ./serve.sh — run in this folder: serve.cmd or: bash serve.sh
set -euo pipefail
PORT="${1:-8765}"
cd "$(dirname "$0")"
if command -v python3 >/dev/null 2>&1; then
  exec python3 -m http.server "$PORT"
elif command -v python >/dev/null 2>&1; then
  exec python -m http.server "$PORT"
else
  echo "Install Python 3 or run: npx --yes serve -s . -l $PORT" >&2
  exit 1
fi
