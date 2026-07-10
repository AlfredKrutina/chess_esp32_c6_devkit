#!/usr/bin/env bash
# Kořenový wrapper — skutečný skript: scripts/docs/create_pdf_simple.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/create_pdf_simple.sh" "$@"
