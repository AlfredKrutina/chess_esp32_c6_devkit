#!/bin/bash
# Script to generate Doxygen documentation
# ESP32-C6 Chess v2.4
# Run from repo root: ./scripts/docs/generate_docs.sh  (or ./generate_docs.sh)

set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# Output colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}ESP32-C6 Chess v2.4 - Doxygen Documentation${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Check whether Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo -e "${RED}ERROR: Doxygen is not installed!${NC}"
    echo ""
    echo "Install on macOS:"
    echo "  brew install doxygen"
    echo ""
    echo "Install on Linux:"
    echo "  sudo apt-get install doxygen  # Debian/Ubuntu"
    echo "  sudo yum install doxygen     # RHEL/CentOS"
    echo ""
    exit 1
fi

# Check Doxygen version
DOXYGEN_VERSION=$(doxygen --version)
echo -e "${GREEN}✓ Doxygen found: version ${DOXYGEN_VERSION}${NC}"
echo ""

# Check whether Doxyfile exists
if [ ! -f "Doxyfile" ]; then
    echo -e "${RED}ERROR: Doxyfile not found!${NC}"
    exit 1
fi

# Create output directory
echo "Creating output directory..."
mkdir -p docs/doxygen
echo -e "${GREEN}✓ Directory created${NC}"
echo ""

# Generate documentation
echo "Generating documentation..."
echo "This may take several minutes..."
echo ""

if doxygen Doxyfile 2>&1 | tee docs/doxygen/generation.log; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ Documentation generated successfully!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    # Try to compile LaTeX to PDF (if LaTeX is installed)
    if [ -d "docs/doxygen/latex" ] && command -v pdflatex &> /dev/null; then
        echo "Compiling LaTeX to PDF..."
        cd docs/doxygen/latex
        if pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1; then
            if makeindex refman.idx > /dev/null 2>&1; then
                pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
                if [ -f "refman.pdf" ]; then
                    mv refman.pdf ../esp32_chess_v24_documentation.pdf
                    echo -e "${GREEN}✓ PDF generated successfully!${NC}"
                fi
            fi
        fi
        cd - > /dev/null
        echo ""
    fi
    
    echo "Output files:"
    echo "  - HTML documentation (local): ${GREEN}docs/doxygen/html/index.html${NC} (multiple files)"
    if [ -f "docs/doxygen/rtf/refman.rtf" ]; then
        echo "  - RTF documentation:  ${GREEN}docs/doxygen/rtf/refman.rtf${NC} (SINGLE FILE - Word compatible)"
    fi
    if [ -f "docs/doxygen/esp32_chess_v24_documentation.pdf" ]; then
        echo "  - PDF documentation: ${GREEN}docs/doxygen/esp32_chess_v24_documentation.pdf${NC} (SINGLE FILE)"
    fi
    echo "  - Log file:           docs/doxygen/generation.log"
    echo "  - Warnings:           docs/doxygen/doxygen_warnings.log"
    echo ""
    
    # Check warnings
    if [ -f "docs/doxygen/doxygen_warnings.log" ]; then
        WARNING_COUNT=$(grep -c "warning:" docs/doxygen/doxygen_warnings.log 2>/dev/null || echo "0")
        if [ "$WARNING_COUNT" -gt 0 ]; then
            echo -e "${YELLOW}⚠ Found ${WARNING_COUNT} warnings${NC}"
            echo "  See: docs/doxygen/doxygen_warnings.log"
        else
            echo -e "${GREEN}✓ No warnings${NC}"
        fi
    fi
    
    echo ""
    echo "Open documentation:"
    echo ""
    echo "  SINGLE FILE (complete documentation):"
    if [ -f "docs/doxygen/esp32_chess_v24_documentation.pdf" ]; then
        echo "    PDF:  ${GREEN}docs/doxygen/esp32_chess_v24_documentation.pdf${NC}"
        echo "      open docs/doxygen/esp32_chess_v24_documentation.pdf  # macOS"
        echo "      xdg-open docs/doxygen/esp32_chess_v24_documentation.pdf  # Linux"
    else
        echo "    PDF:  ${YELLOW}Not available${NC}"
        echo "      Run: ${GREEN}./create_pdf.sh${NC} to create a PDF"
    fi
    if [ -f "docs/doxygen/rtf/refman.rtf" ]; then
        RTF_SIZE=$(ls -lh docs/doxygen/rtf/refman.rtf | awk '{print $5}')
        echo "    RTF:  ${GREEN}docs/doxygen/rtf/refman.rtf${NC} (Word compatible, $RTF_SIZE)"
        echo "      open docs/doxygen/rtf/refman.rtf  # macOS"
        echo "      xdg-open docs/doxygen/rtf/refman.rtf  # Linux"
        echo "      ${YELLOW}Note:${NC} If RTF will not open, try:"
        echo "        - Open in TextEdit: open -a TextEdit docs/doxygen/rtf/refman.rtf"
        echo "        - Open in Microsoft Word (if installed)"
        echo "        - Create PDF: ./create_pdf.sh"
    fi
    echo ""
    echo "  HTML (multiple files, interactive):"
    echo "    Local:  open docs/doxygen/html/index.html  # macOS"
    echo "             xdg-open docs/doxygen/html/index.html  # Linux"
    echo ""
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗ Error generating documentation!${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "Check the log: docs/doxygen/generation.log"
    exit 1
fi
