# Documentation — repository orientation

Entry point: **[README.md](../README.md)** (hardware, build, task overview).

| Where | What |
|-------|------|
| **[context/](../context/)** | Descriptive Markdown — hardware notes, firmware architecture, app integration, media scripts, diagrams |
| **[doxygen/](../doxygen/)** | Generated C API HTML (after `./scripts/docs/generate_docs.sh`; output is gitignored) |
| **[dokumentace/](../dokumentace/)** | Word documents you author (`.doc` / `.docx`) |
| **[docs/flutter/](flutter/README.md)** | Flutter client overview |
| **[docs/reference/](reference/README.md)** | Index of prose moved under `context/` |

**Typical commands:** `./scripts/render_docs.sh` (Mermaid) · `./scripts/docs/generate_docs.sh` (Doxygen → `doxygen/html/index.html`)
