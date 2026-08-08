#!/usr/bin/env bash
# Root wrapper — real script: scripts/docs/create_pdf_simple.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/create_pdf_simple.sh" "$@"
