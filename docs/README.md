# Dokumentace CZECHMATE v2.4

Tato složka obsahuje projektovou dokumentaci.

## Obsah

| Soubor | Popis |
|--------|--------|
| [WEB_UI_DEPLOY.md](WEB_UI_DEPLOY.md) | Deploy webového UI, embed JS, build a flash |
| [KOMUNIKACE_MEZI_TASKY.md](KOMUNIKACE_MEZI_TASKY.md) | Komunikace mezi FreeRTOS tasky a hardware |
| [coordinates_system.md](coordinates_system.md) | Systém souřadnic a převod šachové notace |

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

- **HTML:** `docs/doxygen/html/index.html` – prohlížeč
- **RTF:** `docs/doxygen/rtf/refman.rtf` – jeden soubor (Word)
- **PDF:** `docs/doxygen/esp32_chess_v24_documentation.pdf` – pokud je nainstalovaný LaTeX

Složka `docs/doxygen/` je generovaná a je v `.gitignore`.
