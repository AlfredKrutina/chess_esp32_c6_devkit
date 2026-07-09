# Dokumentace — orientace v repozitáři

Úplný vstup do projektu je v **[README.md](../README.md)** (hardware, GPIO, tabulka tasků, praktické věci). Ve **`docs/`** je hlubší rozpitváno: diagramy, Flutter, OTA, reference kolem integrace.

**Google Formuláře** (zájem o **CzechMate V2**): [předobjednávka](https://docs.google.com/forms/d/18ns5uSUSzr5zcHsiZwD1HWfY15xBa-folmE-oH86BsY/viewform) · [průzkum zájmu](https://docs.google.com/forms/d/e/1FAIpQLSck_q6sjN1nnUs9aV2CsY0MyPNo9puLcncW603iEJz6BMLjPw/viewform)

**Firmware v repu:** **1.8.0** (prototyp **V1**, reed). **V2** = Hall — viz [reference/HARDWARE_VERZE.md](reference/HARDWARE_VERZE.md).

---

## Typické pořadí čtení

1. [README.md](../README.md) — úvod, build, rychlý přehled.
2. [reference/REPO_LAYOUT.md](reference/REPO_LAYOUT.md) — inventář cest a skupin komponent.
3. [reference/HARDWARE_VERZE.md](reference/HARDWARE_VERZE.md) — V1 (reed) vs V2 (Hall).
4. [diagrams/README.md](diagrams/README.md) — boot, fronty, smyčky tasků, šachové toky.
5. [reference/KOMUNIKACE_MEZI_TASKY.md](reference/KOMUNIKACE_MEZI_TASKY.md) — fronty, mutexy, HW podrobněji.
6. [flutter/README.md](flutter/README.md) — klient, BLE/HTTP.
7. [ota_architecture.md](ota_architecture.md) — OTA firmware na desku.
8. [reference/TROUBLESHOOTING.md](reference/TROUBLESHOOTING.md) — ladění a známé problémy.
9. Doxygen: `./generate_docs.sh` → `docs/doxygen/html/index.html`.

Když měním `.mmd` nebo chci přepsat SVG/HTML diagramů: `./scripts/render_docs.sh`.

---

## Co kde leží (inventář)

| Dokument | Účel |
|----------|----------------|
| [README.md](../README.md) | Úvod, build, odkazy |
| [reference/REPO_LAYOUT.md](reference/REPO_LAYOUT.md) | Inventář repozitáře |
| [reference/TROUBLESHOOTING.md](reference/TROUBLESHOOTING.md) | Ladění, UART, známé limity |
| [reference/PROJECT_NOTES.md](reference/PROJECT_NOTES.md) | Verze, autoři, licence, poznámky |
| [reference/HARDWARE_VERZE.md](reference/HARDWARE_VERZE.md) | V1 vs V2 (reed vs Hall, fw 1.8.0, předobjednávka) |
| [docs/README.md](README.md) | Tenhle rozcestník |
| [diagrams/README.md](diagrams/README.md) | Mermaid / SVG přehled |
| [diagrams/diagrams_mermaid.html](diagrams/diagrams_mermaid.html) | Sekvence (generuje `render_docs.sh`) |
| [diagrams/mermaid_diagrams.txt](diagrams/mermaid_diagrams.txt) | Zdroj pro sekvenční HTML |
| [diagrams/sources/chess_flow_*.mmd](diagrams/sources/) | Šablony tahů, recovery, … |
| [flutter/README.md](flutter/README.md) | Flutter klient |
| [reference/KOMUNIKACE_MEZI_TASKY.md](reference/KOMUNIKACE_MEZI_TASKY.md) | Komunikace tasků |
| [reference/coordinates_system.md](reference/coordinates_system.md) | Notace ↔ řádek/sloupec, LED |
| [reference/WEB_UI_DEPLOY.md](reference/WEB_UI_DEPLOY.md) | Embed web UI, build |
| [reference/CZECHMATE_INTEGRATION_CHECKLIST.md](reference/CZECHMATE_INTEGRATION_CHECKLIST.md) | REST, WS, BLE pro klienty |
| [ota_architecture.md](ota_architecture.md) | OTA: kanály, REST/BLE, Flutter, rollback, flash/kompatibilita, checklist před release |
| [reference/BLENDER_VIDEO_BRIEF.md](reference/BLENDER_VIDEO_BRIEF.md) | Co potřebuju k videím z Blenderu |
| [reference/WEB_MEDIA_BRIEF.md](reference/WEB_MEDIA_BRIEF.md) | Médiá pro web — odkaz na lokální spec v `context/` nebo `gh-pages-ready/` |
| [flutter_czechmate/README.md](../flutter_czechmate/README.md) | Spuštění aplikace |
| [diagrams/DIAGRAM_BACKLOG.local.example.md](diagrams/DIAGRAM_BACKLOG.local.example.md) | Šablona backlogu diagramů |

Lokální poznámky k diagramům si píšu do `docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md` (gitignore). OTA logy a vlastní E2E checklisty mohou být v lokální složce `context/` (v `.gitignore`); sdílená pravda pro OTA je [`docs/ota_architecture.md`](ota_architecture.md).

---

## Jak to sedí v repu (diagram)

```mermaid
%%{init: {'theme':'dark','themeVariables':{'lineColor':'#94a3b8','clusterBkg':'#0f172a','clusterBorder':'#334155','primaryTextColor':'#f1f5f9','titleColor':'#f8fafc'}}}%%
flowchart TB
  subgraph FW["Firmware ESP-IDF"]
    M[main/]:::fw
    C[components/]:::fw
    CM[sdkconfig · CMake]:::fw
  end
  subgraph APP["Klient"]
    FL[flutter_czechmate/]:::app
  end
  subgraph DOC["docs/"]
    DG[diagrams/]:::doc
    RF[reference/]:::doc
    DF[flutter/]:::doc
  end
  FW <-->|BLE · HTTP| FL
  DOC -.-> FW
  DOC -.-> FL

  classDef fw fill:#1e3a8a,stroke:#38bdf8,stroke-width:2px,color:#e0f2fe
  classDef app fill:#581c87,stroke:#c084fc,stroke-width:2px,color:#f3e8ff
  classDef doc fill:#14532d,stroke:#4ade80,stroke-width:2px,color:#bbf7d0
```

| Cesta | Obsah |
|-------|--------|
| `main/` | Boot, fronty, start tasků |
| `components/` | `game_task`, `led_task`, `matrix_task`, `uart_task`, `web_server_task`, `ble_task`, … |
| `flutter_czechmate/lib/` | UI, Riverpod, BLE/API |
| `docs/diagrams/` | `sources/*.mmd`, SVG, sekvenční HTML |
| `docs/reference/` | Delší texty — viz [reference/README.md](reference/README.md) |
| `docs/ota_architecture.md` | OTA ESP32 ↔ Flutter |
| `docs/flutter/` | Přehled aplikace |
| `context/` (gitignore) | Lokální podklady pro AI |
| `scripts/` | `render_docs.sh`, `docs/` — viz [scripts/README.md](../scripts/README.md) |
| `generate_docs.sh`, `Doxyfile` | C API HTML (wrapper → `scripts/docs/`) |

---

## Co dokumentace schválně nedělá

- `game_task.c` je obří — celý proud řeším radši přes diagramy a Doxygen; úplný výpis funkcí je v HTML po `generate_docs.sh`.
- Časové chování partie nejlíp sedí z kombinace diagramů, textu v KOMUNIKACE, logů z desky a testů.

---

## Typické příkazy

| Co | Příkaz |
|----|--------|
| Firmware | `idf.py build` (v aktivovaném ESP-IDF prostředí) |
| Flutter | `cd flutter_czechmate && flutter pub get && flutter run` |
| Diagramy | `./scripts/render_docs.sh` |
| Doxygen | `./generate_docs.sh` |
