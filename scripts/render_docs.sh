#!/usr/bin/env bash
# Přegeneruje diagramy a HTML dokumentaci v repu (bez Doxygenu).
# Použití: z kořene repa ./scripts/render_docs.sh
# Volitelně SVG/PNG: npm i -g @mermaid-js/mermaid-cli  → příkaz mmdc

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Puppeteer (mmdc) na GitHub Actions: bez user namespace → „No usable sandbox“.
MERMAID_PUPPETEER_CFG=""
if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
  MERMAID_PUPPETEER_CFG="$ROOT/scripts/mermaid-puppeteer-ci.json"
  if [[ -f "$MERMAID_PUPPETEER_CFG" ]]; then
    echo "==> CI: mermaid-cli použije $MERMAID_PUPPETEER_CFG (--no-sandbox)"
  fi
fi

echo "==> generate_mermaid_html.py → diagrams_mermaid.html + extracted/*.mmd"
python3 generate_mermaid_html.py --export-dir docs/diagrams/extracted

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

render_mmd() {
  local src="$1"
  local base
  base="$(basename "$src" .mmd)"
  local out_dir="docs/diagrams"
  local svg="${out_dir}/${base}.svg"
  local png="${out_dir}/${base}.png"
  local -a pp=()
  if [[ -n "$MERMAID_PUPPETEER_CFG" ]]; then
    pp=(-p "$MERMAID_PUPPETEER_CFG")
  fi
  if command -v mmdc >/dev/null 2>&1; then
    echo "==> mmdc → $svg (+ PNG)"
    mmdc "${pp[@]}" -i "$src" -o "$svg" -b transparent
    mmdc "${pp[@]}" -i "$src" -o "$png" -b transparent -w 1800 2>/dev/null || true
  elif command -v npx >/dev/null 2>&1; then
    echo "==> npx mermaid-cli → $svg (+ PNG)"
    npx --yes @mermaid-js/mermaid-cli "${pp[@]}" -i "$src" -o "$svg" -b transparent
    npx --yes @mermaid-js/mermaid-cli "${pp[@]}" -i "$src" -o "$png" -b transparent -w 1800 2>/dev/null || true
  else
    echo "==> Přeskočeno SVG/PNG pro $base (nainstaluj mermaid-cli nebo npx)"
    return 0
  fi
}

shopt -s nullglob
for MM_SRC in docs/diagrams/sources/*.mmd; do
  render_mmd "$MM_SRC"
done
shopt -u nullglob

if ! command -v mmdc >/dev/null 2>&1 && ! command -v npx >/dev/null 2>&1; then
  echo "==> Celkově přeskočeny SVG/PNG (chybí mmdc i npx)"
fi

echo "Hotovo."
