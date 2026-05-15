#!/usr/bin/env bash
# Lokální náhled downloads.html přes HTTP (ne file:// — videa a relativní cesty).
# Na Windows PowerShell nepoužívej ./serve.sh — spusť ve stejné složce: serve.cmd nebo: bash serve.sh
set -euo pipefail
PORT="${1:-8765}"
cd "$(dirname "$0")"
if command -v python3 >/dev/null 2>&1; then
  exec python3 -m http.server "$PORT"
elif command -v python >/dev/null 2>&1; then
  exec python -m http.server "$PORT"
else
  echo "Nainstaluj Python 3 nebo spusť: npx --yes serve -s . -l $PORT" >&2
  exit 1
fi
