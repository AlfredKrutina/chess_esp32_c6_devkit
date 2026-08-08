# Repository scripts

All automation scripts live under `scripts/`. From the repo root, **thin wrappers** also work (`./generate_docs.sh`, `./generate_mermaid_html.py`, …) for backward compatibility.

## Documentation (`scripts/docs/`)

| Script | Purpose | Command from root |
|--------|---------|-------------------|
| `generate_docs.sh` | Doxygen HTML/RTF/PDF | `./generate_docs.sh` or `./scripts/docs/generate_docs.sh` |
| `generate_mermaid_html.py` | Mermaid HTML + export `.mmd` | `python3 generate_mermaid_html.py` or `python3 scripts/docs/generate_mermaid_html.py` |
| `create_pdf.sh` | PDF from Doxygen RTF/LaTeX | `./create_pdf.sh` |
| `create_pdf_simple.sh` | PDF from RTF (macOS) | `./create_pdf_simple.sh` |

Diagrams (Mermaid SVG/PNG + HTML): `./scripts/render_docs.sh` — calls `scripts/docs/generate_mermaid_html.py`.

## Build (`scripts/`)

| Script | Purpose |
|--------|---------|
| `idf_build.sh` | ESP-IDF build wrapper |
| `build_stm32_embedded.sh` | Demo STM32 Hall → `embedded/stm32_fw_embedded.bin` |
| `render_docs.sh` | Regenerate diagrams |
| `test_opening_api.sh` | HTTP smoke test `POST /api/game/opening` on the board |

BLE-only firmware (no HTTP):  
`idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ble_only" build`

Hall V2 + STM32 auto-flash:  
`idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build flash`  
Wiring: [docs/reference/WIRING_ESP_STM4.md](../docs/reference/WIRING_ESP_STM4.md)

## Hardware testing

| Document | Purpose |
|----------|---------|
| [docs/testing/MANUAL_TEST_CHECKLIST.md](../docs/testing/MANUAL_TEST_CHECKLIST.md) | Opening Trainer v1.0 release gate on physical board |

## Maintenance (`scripts/maintenance/`)

| Script | Purpose |
|--------|---------|
| `delete_dead_castling.sh` | **Deprecated** — do not delete without approval |

## CI

- [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml) — `scripts/docs/generate_docs.sh` + `scripts/render_docs.sh`
- [`.github/workflows/docs-diagrams.yml`](../.github/workflows/docs-diagrams.yml) — `scripts/render_docs.sh`
