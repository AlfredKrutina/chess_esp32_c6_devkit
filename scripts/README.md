# Skripty repozitáře

Všechny automatizační skripty jsou pod `scripts/`. Z kořene repa fungují i **tenké wrappery** (`./generate_docs.sh`, `./generate_mermaid_html.py`, …) pro zpětnou kompatibilitu.

## Dokumentace (`scripts/docs/`)

| Skript | Účel | Příkaz z kořene |
|--------|------|-----------------|
| `generate_docs.sh` | Doxygen HTML/RTF/PDF | `./generate_docs.sh` nebo `./scripts/docs/generate_docs.sh` |
| `generate_mermaid_html.py` | Mermaid HTML + export `.mmd` | `python3 generate_mermaid_html.py` nebo `python3 scripts/docs/generate_mermaid_html.py` |
| `create_pdf.sh` | PDF z Doxygen RTF/LaTeX | `./create_pdf.sh` |
| `create_pdf_simple.sh` | PDF z RTF (macOS) | `./create_pdf_simple.sh` |

Diagramy (Mermaid SVG/PNG + HTML): `./scripts/render_docs.sh` — volá `scripts/docs/generate_mermaid_html.py`.

## Build (`scripts/`)

| Skript | Účel |
|--------|------|
| `idf_build.sh` | ESP-IDF build wrapper |
| `render_docs.sh` | Přegenerování diagramů |

## Údržba (`scripts/maintenance/`)

| Skript | Účel |
|--------|------|
| `delete_dead_castling.sh` | **Deprecated** — nesmazat bez schválení |

## CI

- [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml) — `scripts/docs/generate_docs.sh` + `scripts/render_docs.sh`
- [`.github/workflows/docs-diagrams.yml`](../.github/workflows/docs-diagrams.yml) — `scripts/render_docs.sh`
