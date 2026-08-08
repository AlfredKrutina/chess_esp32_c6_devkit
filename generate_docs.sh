#!/usr/bin/env bash
# Root wrapper — real script: scripts/docs/generate_docs.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/generate_docs.sh" "$@"
