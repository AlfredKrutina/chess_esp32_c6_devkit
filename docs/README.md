# Dokumentace ESP32-C6 Chess v2.4

Tato složka obsahuje veškerou projektovou dokumentaci organizovanou do logických kategorií.

## Struktura dokumentace

### 📁 bugs/
Dokumentace o nalezených a opravených chybách:
- `ALL_BUGS_FOUND.md` - Kompletní seznam všech nalezených chyb
- `BUG_9_EN_PASSANT_CRITICAL.md` - Kritická chyba s en passant
- `COMPLETE_BUG_LIST.md` - Úplný seznam chyb
- `CRITICAL_BUGS_CASTLING_PROMOTION.md` - Kritické chyby v rošádě a promoci
- `FINAL_BUGS_FIXED.md` - Finální seznam opravených chyb
- `ULTIMATE_BUG_FIXES_SUMMARY.md` - Shrnutí všech oprav

### 📁 analysis/
Technické analýzy a rozbory:
- `COMPLETE_ANALYSIS_ALL_RULES.md` - Kompletní analýza všech šachových pravidel
- `ESP32C6_COMPLETE_PIN_ANALYSIS.md` - Kompletní analýza pinů ESP32-C6
- `ESP32C6_PIN_ANALYSIS.md` - Analýza pinů ESP32-C6
- `MATRIX_SCAN_ANALYSIS.md` - Analýza skenování matice
- `VALIDATION_ANALYSIS.md` - Analýza validace

### 📁 planning/
Plány a návrhy:
- `CHESS_IMPROVEMENTS_PLAN.md` - Plán vylepšení šachového systému
- `CZECHMATE_LEARNING_PLAN.md` - Plán učení CzechMate
- `WEB_SERVER_IMPLEMENTATION_PLAN.md` - Plán implementace web serveru

### 📁 hardware/
Hardwarová dokumentace:
- `HARDWARE_WIRING_GUIDE.md` - Průvodce zapojením hardwaru
- `ZAPOJENI.md` - Zapojení (česky)

### 📁 web_server/
Dokumentace web serveru:
- `BUILD_SUCCESS_WEB_SERVER.md` - Úspěšné sestavení web serveru
- `GREEK_STYLE_WEB_SERVER.md` - Řecký styl web serveru
- `WEB_ENHANCEMENTS_IMPLEMENTED.md` - Implementovaná vylepšení
- `WEB_MINIMALISTIC_DESIGN.md` - Minimalistický design
- `WEB_SERVER_ARCHITECTURE.md` - Architektura web serveru
- `WEB_SERVER_BUILD_STATUS.md` - Status sestavení
- `WEB_SERVER_CLEAN_VERSION.md` - Čistá verze
- `WEB_SERVER_FIXES_ANALYSIS.md` - Analýza oprav
- `WEB_SERVER_FIXES_APPLIED.md` - Aplikované opravy
- `WEB_SERVER_IMPLEMENTATION_COMPLETE.md` - Dokončená implementace
- `WEB_SERVER_PROBLEMS_ANALYSIS.md` - Analýza problémů

### 📁 validation/
Validační dokumentace:
- `FINAL_VALIDATION_SUMMARY.md` - Finální shrnutí validace
- `VALIDATION_FIXES_SUMMARY.md` - Shrnutí validačních oprav
- `VALIDATION_FIXES_v2.4.1.md` - Validační opravy v2.4.1

### 📁 improvements/
Dokumentace vylepšení:
- `GAME_LOGIC_IMPROVEMENTS.md` - Vylepšení herní logiky
- `IMMEDIATE_IMPROVEMENTS.md` - Okamžitá vylepšení

### 📁 design/
Design dokumentace:
- `CZECHMATE_GREEK_DESIGN.md` - Řecký design CzechMate

### 📁 tests/
Testovací dokumentace:
- `PROMOTION_SYSTEM_ADVANCED_TESTS.md` - Pokročilé testy systému promoce
- `PROMOTION_SYSTEM_TEST_SCENARIOS.md` - Testovací scénáře systému promoce

### 📁 archive/
Archivované a backup soubory:
- Backup soubory (.bak, .bak2, atd.)
- Staré verze souborů
- Dočasné soubory

### 📁 doxygen/
**Výstupní složka pro Doxygen dokumentaci** (generovaná automaticky)
- `html/` - HTML výstup dokumentace (více souborů, interaktivní)
- `rtf/refman.rtf` - **JEDEN SOUBOR** - RTF dokumentace (kompatibilní s Microsoft Word)
- `esp32_chess_v24_documentation.pdf` - **JEDEN SOUBOR** - PDF dokumentace (pokud je LaTeX nainstalovaný)
- `latex/` - LaTeX zdrojové soubory (pro generování PDF)
- `doxygen_warnings.log` - Log varování
- `generation.log` - Log generování

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

