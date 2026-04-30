# Dokumentace firmware CZECHMATE

Verze projektu je v kořenovém [`CMakeLists.txt`](../CMakeLists.txt) (`PROJECT_VERSION`). Tento adresář doplňuje kořenové [`README.md`](../README.md).

| Složka / soubor | Účel |
|-----------------|------|
| **[diagrams/](diagrams/)** | Přehledové diagramy (Mermaid), SVG/PNG z `.mmd`, HTML knihovna sekvencí |
| **[reference/](reference/)** | Delší texty: komunikace tasků, web UI, souřadnice, checklist integrace |

## Diagramy — kde začít

1. **[diagrams/README.md](diagrams/README.md)** — nadpisy + diagramy (tasky, fronty, boot, tah z matice).
2. Lokální HTML se všemi sekvenčními diagramy: po `./scripts/render_docs.sh` otevři [`diagrams/diagrams_mermaid.html`](diagrams/diagrams_mermaid.html).
3. Zdroj sekvencí pro HTML / úpravy: [`diagrams/mermaid_diagrams.txt`](diagrams/mermaid_diagrams.txt).
4. Jeden dlouhý sekvenční „hlavní smyčka“ (ladění): [`diagrams/main_flow_diagram.txt`](diagrams/main_flow_diagram.txt).

## Reference

- [Komunikace mezi tasky](reference/KOMUNIKACE_MEZI_TASKY.md)
- [Souřadnice a mapování](reference/coordinates_system.md)
- [Web UI / deploy](reference/WEB_UI_DEPLOY.md)
- [Checklist integrace](reference/CZECHMATE_INTEGRATION_CHECKLIST.md)
