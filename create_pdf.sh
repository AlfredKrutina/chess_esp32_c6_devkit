#!/usr/bin/env bash
# Kořenový wrapper — skutečný skript: scripts/docs/create_pdf.sh
exec "$(cd "$(dirname "$0")" && pwd)/scripts/docs/create_pdf.sh" "$@"
