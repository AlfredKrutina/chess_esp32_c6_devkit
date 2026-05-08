# Lokální backlog diagramů (vzor)

Soubor **`LOCAL_DIAGRAM_BACKLOG.md`** mám v **`.gitignore`** — dlouhé poznámky a rozpracované seznamy nepatří na remote.

**Jak si ho založím**

1. Zkopíruju tenhle vzor:

   `cp docs/diagrams/DIAGRAM_BACKLOG.local.example.md docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`

2. Do **`LOCAL_DIAGRAM_BACKLOG.md`** si dopisuju vlastní TODO (firmware / Flutter / CI / GPIO), paletu barev, zkopírovatelné `%%{init}%%` / `classDef` a odkazy na zdrojové soubory.

Po čistém klonu si soubor stejně musím vytvořit u sebe na disku — git ho nesdílí.

Veřejně v repu zůstávají hotové diagramy v **[README.md](README.md)** tady a v **`sources/*.mmd`** (+ SVG po `./scripts/render_docs.sh`). Hlavní mapa dokumentace: **[`docs/README.md`](../README.md)**.
