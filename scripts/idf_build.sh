#!/usr/bin/env bash
# Run from project root with ESP-IDF loaded: source $IDF_PATH/export.sh && ./scripts/idf_build.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
idf.py build "$@"
