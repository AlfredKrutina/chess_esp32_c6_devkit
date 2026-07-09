#!/usr/bin/env bash
# Kořenový wrapper — skutečný skript: scripts/docs/generate_docs.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/generate_docs.sh" "$@"
