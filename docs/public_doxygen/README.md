# CZECHMATE - Doxygen Dokumentace

Tato složka obsahuje veřejnou Doxygen dokumentaci pro GitHub.

## 📖 Struktura

- `html/` - **Plná HTML dokumentace** (všechny soubory potřebné pro index.html)
  - `!index.html` - ⭐ **Hlavní stránka dokumentace** (řadí se jako první!)
  - `index.html` - Přesměrování na !index.html (pro kompatibilitu)
  - Všechny HTML soubory, CSS, JS, search složka
  - Plně funkční dokumentace s odkazy a vyhledáváním
  
- `rtf/` - **RTF dokumentace pro Microsoft Word**
  - `refman.rtf` - Kompletní dokumentace v jednom souboru (Word kompatibilní)

## 🔄 Aktualizace

Dokumentace je automaticky aktualizována při spuštění `./generate_docs.sh` v rootu projektu.

## 📚 Použití

**HTML dokumentace:**
```bash
# Otevřít !index.html (řadí se jako první) nebo index.html (přesměruje)
open docs/public_doxygen/html/!index.html  # macOS (hlavní stránka)
open docs/public_doxygen/html/index.html  # macOS (přesměruje na !index.html)
xdg-open docs/public_doxygen/html/!index.html  # Linux (hlavní stránka)
xdg-open docs/public_doxygen/html/index.html  # Linux (přesměruje na !index.html)
```

**RTF dokumentace:**
```bash
open docs/public_doxygen/rtf/refman.rtf  # macOS (Word)
xdg-open docs/public_doxygen/rtf/refman.rtf  # Linux
```

---

**Verze dokumentace:** 2.4.0  
**Poslední aktualizace:** Automaticky při generování dokumentace
