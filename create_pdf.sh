#!/bin/bash
# Skript pro vytvoření PDF z RTF nebo LaTeX dokumentace
# ESP32-C6 Chess v2.4

set -e

# Barvy pro výstup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

RTF_FILE="docs/doxygen/rtf/refman.rtf"
LATEX_DIR="docs/doxygen/latex"
PDF_OUTPUT="docs/doxygen/esp32_chess_v24_documentation.pdf"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Vytváření PDF dokumentace${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Metoda 1: Zkusit zkompilovat LaTeX do PDF
if [ -d "$LATEX_DIR" ] && command -v pdflatex &> /dev/null; then
    echo -e "${GREEN}Metoda 1: Kompilace LaTeX do PDF...${NC}"
    cd "$LATEX_DIR"
    
    if pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1; then
        if command -v makeindex &> /dev/null; then
            makeindex refman.idx > /dev/null 2>&1 || true
        fi
        pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
        
        if [ -f "refman.pdf" ]; then
            mv refman.pdf "../../esp32_chess_v24_documentation.pdf"
            cd - > /dev/null
            echo -e "${GREEN}✓ PDF úspěšně vytvořen z LaTeX!${NC}"
            echo "  Soubor: $PDF_OUTPUT"
            exit 0
        fi
    fi
    cd - > /dev/null
    echo -e "${YELLOW}⚠ LaTeX kompilace selhala${NC}"
    echo ""
fi

# Metoda 2: Použít macOS AppleScript pro konverzi RTF na PDF
if [ -f "$RTF_FILE" ] && [ "$(uname)" = "Darwin" ]; then
    echo -e "${GREEN}Metoda 2: Konverze RTF na PDF pomocí macOS (TextEdit/Preview)...${NC}"
    
    # Použít osascript pro automatickou konverzi přes TextEdit
    osascript <<EOF 2>/dev/null || true
tell application "TextEdit"
    open POSIX file "$(pwd)/$RTF_FILE"
    set theDoc to front document
    save theDoc in "$(pwd)/$PDF_OUTPUT" as "PDF"
    close theDoc
end tell
EOF
    
    if [ -f "$PDF_OUTPUT" ]; then
        echo -e "${GREEN}✓ PDF vytvořen pomocí macOS TextEdit!${NC}"
        echo "  Soubor: $PDF_OUTPUT"
        exit 0
    fi
    
    # Alternativně použít Preview přes AppleScript
    osascript <<EOF 2>/dev/null || true
tell application "Preview"
    open POSIX file "$(pwd)/$RTF_FILE"
    set theDoc to front document
    save theDoc in "$(pwd)/$PDF_OUTPUT" as "PDF"
    close theDoc
end tell
EOF
    
    if [ -f "$PDF_OUTPUT" ]; then
        echo -e "${GREEN}✓ PDF vytvořen pomocí macOS Preview!${NC}"
        echo "  Soubor: $PDF_OUTPUT"
        exit 0
    fi
    
    echo -e "${YELLOW}⚠ Automatická konverze přes AppleScript selhala${NC}"
    echo ""
fi

# Metoda 3: Použít Python s reportlab nebo jinou knihovnou
if [ -f "$RTF_FILE" ] && command -v python3 &> /dev/null; then
    echo -e "${GREEN}Metoda 3: Zkouším Python konverzi...${NC}"
    
    # Zkusit použít pypandoc nebo jiné nástroje
    if python3 -c "import pypandoc" 2>/dev/null; then
        echo "Konverze pomocí pypandoc..."
        python3 -c "
import pypandoc
pypandoc.convert_file('$RTF_FILE', 'pdf', outputfile='$PDF_OUTPUT')
" && echo -e "${GREEN}✓ PDF vytvořen pomocí pypandoc!${NC}" && exit 0
    fi
    
    # Zkusit použít docx2pdf (pokud je RTF převedený na DOCX)
    echo -e "${YELLOW}⚠ Python knihovny pro RTF->PDF nejsou dostupné${NC}"
    echo ""
fi

# Metoda 4: Použít LibreOffice (pokud je nainstalovaný)
if [ -f "$RTF_FILE" ] && command -v libreoffice &> /dev/null; then
    echo -e "${GREEN}Metoda 4: Konverze RTF na PDF pomocí LibreOffice...${NC}"
    cd docs/doxygen/rtf
    libreoffice --headless --convert-to pdf --outdir ../.. refman.rtf 2>/dev/null
    if [ -f "../../refman.pdf" ]; then
        mv ../../refman.pdf ../../esp32_chess_v24_documentation.pdf
        cd - > /dev/null
        echo -e "${GREEN}✓ PDF vytvořen pomocí LibreOffice!${NC}"
        echo "  Soubor: $PDF_OUTPUT"
        exit 0
    fi
    cd - > /dev/null
fi

# Metoda 5: Instrukce pro manuální konverzi
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Automatická konverze není dostupná${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""
echo "Možnosti pro vytvoření PDF:"
echo ""
echo -e "${BLUE}1. Otevřít RTF v Microsoft Word a uložit jako PDF:${NC}"
echo "   open $RTF_FILE"
echo "   (Pak: Soubor -> Uložit jako -> PDF)"
echo ""
echo -e "${BLUE}2. Otevřít RTF v TextEdit a tisknout do PDF (macOS):${NC}"
echo "   open -a TextEdit $RTF_FILE"
echo "   (Pak: Soubor -> Tisk -> Uložit jako PDF)"
echo ""
echo -e "${BLUE}3. Nainstalovat LaTeX a zkompilovat:${NC}"
echo "   brew install --cask mactex"
echo "   cd docs/doxygen/latex"
echo "   pdflatex refman.tex"
echo "   makeindex refman.idx"
echo "   pdflatex refman.tex"
echo ""
echo -e "${BLUE}4. Nainstalovat LibreOffice:${NC}"
echo "   brew install --cask libreoffice"
echo "   ./create_pdf.sh"
echo ""
echo -e "${BLUE}5. Nainstalovat pandoc:${NC}"
echo "   brew install pandoc"
echo "   pandoc $RTF_FILE -o $PDF_OUTPUT"
echo ""

exit 1

