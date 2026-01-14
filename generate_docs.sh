#!/bin/bash
# Skript pro generování Doxygen dokumentace
# ESP32-C6 Chess v2.4

set -e

# Barvy pro výstup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}ESP32-C6 Chess v2.4 - Doxygen Dokumentace${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Zkontrolovat, jestli je Doxygen nainstalovaný
if ! command -v doxygen &> /dev/null; then
    echo -e "${RED}CHYBA: Doxygen není nainstalovaný!${NC}"
    echo ""
    echo "Instalace na macOS:"
    echo "  brew install doxygen"
    echo ""
    echo "Instalace na Linux:"
    echo "  sudo apt-get install doxygen  # Debian/Ubuntu"
    echo "  sudo yum install doxygen     # RHEL/CentOS"
    echo ""
    exit 1
fi

# Zkontrolovat verzi Doxygen
DOXYGEN_VERSION=$(doxygen --version)
echo -e "${GREEN}✓ Doxygen nalezen: verze ${DOXYGEN_VERSION}${NC}"
echo ""

# Zkontrolovat, jestli existuje Doxyfile
if [ ! -f "Doxyfile" ]; then
    echo -e "${RED}CHYBA: Doxyfile nenalezen!${NC}"
    exit 1
fi

# Vytvořit výstupní adresář
echo "Vytváření výstupního adresáře..."
mkdir -p docs/doxygen
echo -e "${GREEN}✓ Adresář vytvořen${NC}"
echo ""

# Vygenerovat dokumentaci
echo "Generování dokumentace..."
echo "To může trvat několik minut..."
echo ""

if doxygen Doxyfile 2>&1 | tee docs/doxygen/generation.log; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ Dokumentace úspěšně vygenerována!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    # Zkusit zkompilovat LaTeX do PDF (pokud je LaTeX nainstalovaný)
    if [ -d "docs/doxygen/latex" ] && command -v pdflatex &> /dev/null; then
        echo "Kompilace LaTeX do PDF..."
        cd docs/doxygen/latex
        if pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1; then
            if makeindex refman.idx > /dev/null 2>&1; then
                pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
                if [ -f "refman.pdf" ]; then
                    mv refman.pdf ../esp32_chess_v24_documentation.pdf
                    echo -e "${GREEN}✓ PDF úspěšně vygenerován!${NC}"
                fi
            fi
        fi
        cd - > /dev/null
        echo ""
    fi
    
    # Zkopírovat plnou HTML dokumentaci do docs/public_doxygen/html/ a RTF do docs/public_doxygen/rtf/
    if [ -d "docs/doxygen/html" ] && [ -f "docs/doxygen/html/index.html" ]; then
        echo "Kopírování dokumentace do docs/public_doxygen/ (pro GitHub)..."
        rm -rf docs/public_doxygen/*
        mkdir -p docs/public_doxygen/html
        mkdir -p docs/public_doxygen/rtf
        
        # Kopírovat celou HTML dokumentaci
        echo "  📋 Kopíruji HTML dokumentaci..."
        cp -r docs/doxygen/html/* docs/public_doxygen/html/
        
        # Přejmenovat index.html na !index.html, aby bylo vidět jako první
        if [ -f "docs/public_doxygen/html/index.html" ]; then
            mv docs/public_doxygen/html/index.html docs/public_doxygen/html/!index.html
            # Vytvořit index.html jako přesměrování na !index.html (pro kompatibilitu)
            cat > docs/public_doxygen/html/index.html << 'HTML_EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="0; url=!index.html">
    <title>Dokumentace - Přesměrování</title>
</head>
<body>
    <p>Přesměrování na <a href="!index.html">hlavní stránku dokumentace</a>...</p>
    <script>window.location.href = '!index.html';</script>
</body>
</html>
HTML_EOF
        fi
        
        # Kopírovat RTF dokumentaci
        if [ -f "docs/doxygen/rtf/refman.rtf" ]; then
            echo "  📄 Kopíruji RTF dokumentaci..."
            cp docs/doxygen/rtf/refman.rtf docs/public_doxygen/rtf/
        fi
        
        # Vytvořit README.md
        cat > docs/public_doxygen/README.md << 'EOF'
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
EOF
        echo -e "${GREEN}✓ Dokumentace zkopírována do docs/public_doxygen/${NC}"
        echo ""
    fi
    
    echo "Výstupní soubory:"
    echo "  - HTML dokumentace (lokální): ${GREEN}docs/doxygen/html/index.html${NC} (více souborů)"
    echo "  - HTML dokumentace (GitHub):   ${GREEN}docs/public_doxygen/html/index.html${NC} (plná verze, viditelné na GitHubu)"
    if [ -f "docs/public_doxygen/rtf/refman.rtf" ]; then
        echo "  - RTF dokumentace (GitHub):    ${GREEN}docs/public_doxygen/rtf/refman.rtf${NC} (Word kompatibilní)"
    fi
    if [ -f "docs/doxygen/rtf/refman.rtf" ]; then
        echo "  - RTF dokumentace:  ${GREEN}docs/doxygen/rtf/refman.rtf${NC} (JEDEN SOUBOR - Word kompatibilní)"
    fi
    if [ -f "docs/doxygen/esp32_chess_v24_documentation.pdf" ]; then
        echo "  - PDF dokumentace: ${GREEN}docs/doxygen/esp32_chess_v24_documentation.pdf${NC} (JEDEN SOUBOR)"
    fi
    echo "  - Log soubor:       docs/doxygen/generation.log"
    echo "  - Varování:         docs/doxygen/doxygen_warnings.log"
    echo ""
    
    # Zkontrolovat varování
    if [ -f "docs/doxygen/doxygen_warnings.log" ]; then
        WARNING_COUNT=$(grep -c "warning:" docs/doxygen/doxygen_warnings.log 2>/dev/null || echo "0")
        if [ "$WARNING_COUNT" -gt 0 ]; then
            echo -e "${YELLOW}⚠ Nalezeno ${WARNING_COUNT} varování${NC}"
            echo "  Podívejte se na: docs/doxygen/doxygen_warnings.log"
        else
            echo -e "${GREEN}✓ Žádná varování${NC}"
        fi
    fi
    
    echo ""
    echo "Otevření dokumentace:"
    echo ""
    echo "  JEDEN SOUBOR (kompletní dokumentace):"
    if [ -f "docs/doxygen/esp32_chess_v24_documentation.pdf" ]; then
        echo "    PDF:  ${GREEN}docs/doxygen/esp32_chess_v24_documentation.pdf${NC}"
        echo "      open docs/doxygen/esp32_chess_v24_documentation.pdf  # macOS"
        echo "      xdg-open docs/doxygen/esp32_chess_v24_documentation.pdf  # Linux"
    else
        echo "    PDF:  ${YELLOW}Není k dispozici${NC}"
        echo "      Spusťte: ${GREEN}./create_pdf.sh${NC} pro vytvoření PDF"
    fi
    if [ -f "docs/doxygen/rtf/refman.rtf" ]; then
        RTF_SIZE=$(ls -lh docs/doxygen/rtf/refman.rtf | awk '{print $5}')
        echo "    RTF:  ${GREEN}docs/doxygen/rtf/refman.rtf${NC} (Word kompatibilní, $RTF_SIZE)"
        echo "      open docs/doxygen/rtf/refman.rtf  # macOS"
        echo "      xdg-open docs/doxygen/rtf/refman.rtf  # Linux"
        echo "      ${YELLOW}Poznámka:${NC} Pokud RTF nejde otevřít, zkuste:"
        echo "        - Otevřít v TextEdit: open -a TextEdit docs/doxygen/rtf/refman.rtf"
        echo "        - Otevřít v Microsoft Word (pokud je nainstalovaný)"
        echo "        - Vytvořit PDF: ./create_pdf.sh"
    fi
    echo ""
    echo "  HTML (více souborů, interaktivní):"
    echo "    Lokální:  open docs/doxygen/html/index.html  # macOS"
    echo "             xdg-open docs/doxygen/html/index.html  # Linux"
    echo "    GitHub:  open docs/public/index.html  # macOS (viditelné na GitHubu)"
    echo "            xdg-open docs/public/index.html  # Linux"
    echo ""
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗ Chyba při generování dokumentace!${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "Zkontrolujte log: docs/doxygen/generation.log"
    exit 1
fi

