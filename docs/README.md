# Dokumentace projektu CZECHMATE (v2.5+)

Centrální rozcestník pro **lidsky psanou** dokumentaci. Zdrojový kód doplňuje generovaná **Doxygen** dokumentace (viz níže).

---

## Plánování a návrhy (aktuální)

| Dokument | Popis |
|----------|--------|
| [planning/CZECHMATE_MASTERPLAN.md](planning/CZECHMATE_MASTERPLAN.md) | **Hlavní masterplan** — iOS aplikace, firmware, WebSocket, BLE, Stockfish, rizika |
| [planning/CZECHMATE_COMPLETION_PLAN.md](planning/CZECHMATE_COMPLETION_PLAN.md) | **Dodělávky** — stav hotovo/zbývá, fáze A–D, ESP+iOS, Definition of Done |
| [planning/CZECHMATE_BLE_PRIMARY_IMPLEMENTATION_PLAN.md](planning/CZECHMATE_BLE_PRIMARY_IMPLEMENTATION_PLAN.md) | **BLE primárně, Wi‑Fi fallback** — architektura, firmware GATT, iOS, jednoduché UI, fáze F0–F6 |
| [planning/CZECHMATE_IOS_APP_DESIGN.md](planning/CZECHMATE_IOS_APP_DESIGN.md) | Návrh iOS aplikace, obrazovky, roadmapa v1–v3 |
| [planning/CZECHMATE_PROJECT_ANALYSIS.md](planning/CZECHMATE_PROJECT_ANALYSIS.md) | Analýza firmware, REST API, tasků a strategie integrace |

---

## Referenční příručky (aktivní vývoj)

| Dokument | Popis |
|----------|--------|
| [reference/WEB_UI_DEPLOY.md](reference/WEB_UI_DEPLOY.md) | Deploy webového UI, embed JS, build a flash |
| [reference/KOMUNIKACE_MEZI_TASKY.md](reference/KOMUNIKACE_MEZI_TASKY.md) | Komunikace mezi FreeRTOS tasky a hardware |
| [reference/coordinates_system.md](reference/coordinates_system.md) | Systém souřadnic a převod šachové notace |

---

## Diagramy a vizuály

| Umístění | Popis |
|----------|--------|
| [diagrams/diagrams_mermaid.html](diagrams/diagrams_mermaid.html) | Interaktivní Mermaid — sekvenční diagramy toků |
| [diagrams/mermaid_diagrams.txt](diagrams/mermaid_diagrams.txt) | Zdrojové Mermaid definice (text) |
| [diagrams/main_flow_diagram.txt](diagrams/main_flow_diagram.txt) | Hlavní flow diagram (text) |
| [diagrams/tasks_architecture.png](diagrams/tasks_architecture.png) | Obrázek architektury tasků |

---

## Archiv

Starší analýzy a dílčí poznámky: [archive/README.md](archive/README.md).

---

## Doxygen (API a kód)

Dokumentace ze zdrojového kódu se generuje pomocí [Doxygen](https://www.doxygen.nl/).

**Generování:**

```bash
./generate_docs.sh
```

Nebo přímo:

```bash
doxygen Doxyfile
```

**Výstup:**

- **HTML:** `docs/doxygen/html/index.html` — prohlížeč  
- **RTF:** `docs/doxygen/rtf/refman.rtf` — jeden soubor (Word)  
- **PDF:** `docs/doxygen/esp32_chess_v24_documentation.pdf` — pokud je nainstalovaný LaTeX  

Složka `docs/doxygen/html/` a související generované výstupy jsou v `.gitignore` (kromě případných projektových výjimek).

---

## iOS projekt (Xcode)

Nativní aplikace: složka `CZECHMATE/` v kořeni repozitáře. Koncept a postup jsou v `planning/CZECHMATE_MASTERPLAN.md` a `planning/CZECHMATE_IOS_APP_DESIGN.md`.
