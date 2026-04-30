#!/usr/bin/env bash
# Přegeneruje diagramy a HTML dokumentaci v repu (bez Doxygenu).
# Použití: z kořene repa ./scripts/render_docs.sh
# Volitelně SVG/PNG: npm i -g @mermaid-js/mermaid-cli  → příkaz mmdc

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> generate_mermaid_html.py → docs/diagrams/diagrams_mermaid.html"
python3 generate_mermaid_html.py

echo "==> kopie Mermaid HTML pro gh-pages / doxygen (pokud složky existují)"
if [[ -d gh-pages-ready ]]; then
  cp -f docs/diagrams/diagrams_mermaid.html gh-pages-ready/diagrams_mermaid.html
  echo "    gh-pages-ready/diagrams_mermaid.html"
fi
if [[ -f docs/doxygen/html/index.html ]]; then
  cp -f docs/diagrams/diagrams_mermaid.html docs/doxygen/html/diagrams_mermaid.html
  echo "    docs/doxygen/html/diagrams_mermaid.html"
elif [[ -d docs/doxygen ]]; then
  echo "    (přeskočeno: vygeneruj nejdřív Doxygen — ./generate_docs.sh)"
fi

MM_SRC="docs/diagrams/sources/tasks_architecture.mmd"
MM_SVG="docs/diagrams/tasks_architecture.svg"
MM_PNG="docs/diagrams/tasks_architecture.png"

if command -v mmdc >/dev/null 2>&1; then
  echo "==> mmdc → $MM_SVG (+ PNG)"
  mmdc -i "$MM_SRC" -o "$MM_SVG" -b transparent
  mmdc -i "$MM_SRC" -o "$MM_PNG" -b transparent -w 1800 2>/dev/null || true
elif command -v npx >/dev/null 2>&1; then
  echo "==> npx @mermaid-js/mermaid-cli → $MM_SVG (+ PNG) — první běh stáhne závislosti"
  npx --yes @mermaid-js/mermaid-cli -i "$MM_SRC" -o "$MM_SVG" -b transparent
  npx --yes @mermaid-js/mermaid-cli -i "$MM_SRC" -o "$MM_PNG" -b transparent -w 1800 2>/dev/null || true
else
  echo "==> Přeskočeno SVG/PNG (nainstaluj: npm install -g @mermaid-js/mermaid-cli, nebo zajisti npx)"
fi

echo "Hotovo."
