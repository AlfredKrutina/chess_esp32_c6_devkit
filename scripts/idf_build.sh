#!/usr/bin/env bash
# Spusť z kořene projektu s načteným ESP-IDF: source $IDF_PATH/export.sh && ./scripts/idf_build.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
idf.py build "$@"
