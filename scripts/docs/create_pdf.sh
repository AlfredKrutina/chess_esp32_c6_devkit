#!/bin/bash
# Script to create PDF from RTF or LaTeX documentation
# ESP32-C6 Chess v2.4
# Run from repo root: ./scripts/docs/create_pdf.sh  (or ./create_pdf.sh)

set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# Output colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

RTF_FILE="docs/doxygen/rtf/refman.rtf"
LATEX_DIR="docs/doxygen/latex"
PDF_OUTPUT="docs/doxygen/esp32_chess_v24_documentation.pdf"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Creating PDF documentation${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Method 1: Try compiling LaTeX to PDF
if [ -d "$LATEX_DIR" ] && command -v pdflatex &> /dev/null; then
    echo -e "${GREEN}Method 1: Compiling LaTeX to PDF...${NC}"
    cd "$LATEX_DIR"
    
    if pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1; then
        if command -v makeindex &> /dev/null; then
            makeindex refman.idx > /dev/null 2>&1 || true
        fi
        pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
        
        if [ -f "refman.pdf" ]; then
            mv refman.pdf "../../esp32_chess_v24_documentation.pdf"
            cd - > /dev/null
            echo -e "${GREEN}✓ PDF created successfully from LaTeX!${NC}"
            echo "  File: $PDF_OUTPUT"
            exit 0
        fi
    fi
    cd - > /dev/null
    echo -e "${YELLOW}⚠ LaTeX compilation failed${NC}"
    echo ""
fi

# Method 2: Use macOS AppleScript to convert RTF to PDF
if [ -f "$RTF_FILE" ] && [ "$(uname)" = "Darwin" ]; then
    echo -e "${GREEN}Method 2: Converting RTF to PDF via macOS (TextEdit/Preview)...${NC}"
    
    # Use osascript for automatic conversion via TextEdit
    osascript <<EOF 2>/dev/null || true
tell application "TextEdit"
    open POSIX file "$(pwd)/$RTF_FILE"
    set theDoc to front document
    save theDoc in "$(pwd)/$PDF_OUTPUT" as "PDF"
    close theDoc
end tell
EOF
    
    if [ -f "$PDF_OUTPUT" ]; then
        echo -e "${GREEN}✓ PDF created via macOS TextEdit!${NC}"
        echo "  File: $PDF_OUTPUT"
        exit 0
    fi
    
    # Alternatively use Preview via AppleScript
    osascript <<EOF 2>/dev/null || true
tell application "Preview"
    open POSIX file "$(pwd)/$RTF_FILE"
    set theDoc to front document
    save theDoc in "$(pwd)/$PDF_OUTPUT" as "PDF"
    close theDoc
end tell
EOF
    
    if [ -f "$PDF_OUTPUT" ]; then
        echo -e "${GREEN}✓ PDF created via macOS Preview!${NC}"
        echo "  File: $PDF_OUTPUT"
        exit 0
    fi
    
    echo -e "${YELLOW}⚠ Automatic conversion via AppleScript failed${NC}"
    echo ""
fi

# Method 3: Use Python with reportlab or another library
if [ -f "$RTF_FILE" ] && command -v python3 &> /dev/null; then
    echo -e "${GREEN}Method 3: Trying Python conversion...${NC}"
    
    # Try pypandoc or similar tools
    if python3 -c "import pypandoc" 2>/dev/null; then
        echo "Converting with pypandoc..."
        python3 -c "
import pypandoc
pypandoc.convert_file('$RTF_FILE', 'pdf', outputfile='$PDF_OUTPUT')
" && echo -e "${GREEN}✓ PDF created via pypandoc!${NC}" && exit 0
    fi
    
    # Try docx2pdf (if RTF was converted to DOCX)
    echo -e "${YELLOW}⚠ Python libraries for RTF->PDF are not available${NC}"
    echo ""
fi

# Method 4: Use LibreOffice (if installed)
if [ -f "$RTF_FILE" ] && command -v libreoffice &> /dev/null; then
    echo -e "${GREEN}Method 4: Converting RTF to PDF via LibreOffice...${NC}"
    cd docs/doxygen/rtf
    libreoffice --headless --convert-to pdf --outdir ../.. refman.rtf 2>/dev/null
    if [ -f "../../refman.pdf" ]; then
        mv ../../refman.pdf ../../esp32_chess_v24_documentation.pdf
        cd - > /dev/null
        echo -e "${GREEN}✓ PDF created via LibreOffice!${NC}"
        echo "  File: $PDF_OUTPUT"
        exit 0
    fi
    cd - > /dev/null
fi

# Method 5: Manual conversion instructions
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Automatic conversion is not available${NC}"
echo -e "${YELLOW}========================================${NC}"
echo ""
echo "Options to create a PDF:"
echo ""
echo -e "${BLUE}1. Open RTF in Microsoft Word and save as PDF:${NC}"
echo "   open $RTF_FILE"
echo "   (Then: File -> Save As -> PDF)"
echo ""
echo -e "${BLUE}2. Open RTF in TextEdit and print to PDF (macOS):${NC}"
echo "   open -a TextEdit $RTF_FILE"
echo "   (Then: File -> Print -> Save as PDF)"
echo ""
echo -e "${BLUE}3. Install LaTeX and compile:${NC}"
echo "   brew install --cask mactex"
echo "   cd docs/doxygen/latex"
echo "   pdflatex refman.tex"
echo "   makeindex refman.idx"
echo "   pdflatex refman.tex"
echo ""
echo -e "${BLUE}4. Install LibreOffice:${NC}"
echo "   brew install --cask libreoffice"
echo "   ./create_pdf.sh"
echo ""
echo -e "${BLUE}5. Install pandoc:${NC}"
echo "   brew install pandoc"
echo "   pandoc $RTF_FILE -o $PDF_OUTPUT"
echo ""

exit 1
