# Local diagram backlog (template)

The file **`LOCAL_DIAGRAM_BACKLOG.md`** is in **`.gitignore`** — long notes and draft lists do not belong on the remote.

**How to create it**

1. Copy this template:

   `cp docs/diagrams/DIAGRAM_BACKLOG.local.example.md docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`

2. Add custom TODO items to **`LOCAL_DIAGRAM_BACKLOG.md`** (firmware / Flutter / CI / GPIO), color palettes, copy-pasteable `%%{init}%%` / `classDef`, and links to source files.

After a clean clone, the file must still be created locally on disk — git does not share it.

Finished diagrams in the public repo remain in **[README.md](README.md)** here and in **`sources/*.mmd`** (+ SVG after `./scripts/render_docs.sh`). Main documentation map: **[`docs/README.md`](../README.md)**.
