#!/usr/bin/env bash
# Root wrapper — real script: scripts/docs/create_pdf.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/create_pdf.sh" "$@"
