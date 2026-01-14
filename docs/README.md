# Dokumentace ESP32-C6 Chess v2.4

Tato složka obsahuje veškerou projektovou dokumentaci organizovanou do logických kategorií.

## Struktura dokumentace

### 📁 tests/
Testovací dokumentace:
- `TEST_PLAN.md` - Plán testování
- `TEST_RESULTS.md` - Výsledky testů
- `KNOWN_ISSUES.md` - Známé problémy

### 📁 archive/
Archivované soubory:
- `md_documents/` - Archivované MD dokumenty (historické záznamy, plány)
  - `stability/` - Analýzy stability
  - `summaries/` - Shrnutí a přehledy
  - `main_flow_fixes_plan.md` - Plán oprav diagramu (implementováno)
- Backup soubory (.bak, .bak2, atd.)
- Staré verze souborů

### 📁 doxygen/
**Výstupní složka pro Doxygen dokumentaci** (generovaná automaticky, ignorována v .gitignore)
- `html/` - HTML výstup dokumentace (více souborů, interaktivní)
- `rtf/refman.rtf` - **JEDEN SOUBOR** - RTF dokumentace (kompatibilní s Microsoft Word)
- `esp32_chess_v24_documentation.pdf` - **JEDEN SOUBOR** - PDF dokumentace (pokud je LaTeX nainstalovaný)
- `latex/` - LaTeX zdrojové soubory (pro generování PDF)
- `doxygen_warnings.log` - Log varování
- `generation.log` - Log generování

### 📁 public_doxygen/
**Veřejná Doxygen dokumentace** (viditelná na GitHubu)
- Automaticky zkopírována z `doxygen/` při generování dokumentace
- `html/` - Plná HTML dokumentace (všechny soubory potřebné pro index.html)
- `rtf/` - RTF dokumentace pro Microsoft Word (refman.rtf)

## Generování Doxygen dokumentace

Pro vygenerování finální Doxygen dokumentace použijte:

```bash
./generate_docs.sh
```

Nebo přímo:

```bash
doxygen Doxyfile
```

Výstup bude v:
- `docs/doxygen/html/index.html` - HTML dokumentace (prohlížeč, více souborů)
- `docs/doxygen/rtf/refman.rtf` - **JEDEN SOUBOR** - RTF dokumentace (Microsoft Word)
- `docs/doxygen/esp32_chess_v24_documentation.pdf` - **JEDEN SOUBOR** - PDF dokumentace (pokud je LaTeX nainstalovaný)

**Pro jeden soubor s kompletní dokumentací použijte RTF nebo PDF.**

## Poznámky

- Hlavní README.md zůstává v root adresáři projektu
- Doxygen dokumentace se generuje do `docs/doxygen/`
- Archivované soubory jsou v `docs/archive/` a nejsou součástí Doxygen dokumentace

