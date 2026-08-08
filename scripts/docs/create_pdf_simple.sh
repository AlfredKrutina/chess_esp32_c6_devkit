#!/bin/bash
# Simple script to create PDF from RTF using macOS
# ESP32-C6 Chess v2.4
# Run from repo root: ./scripts/docs/create_pdf_simple.sh  (or ./create_pdf_simple.sh)

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

RTF_FILE="docs/doxygen/rtf/refman.rtf"
PDF_OUTPUT="docs/doxygen/esp32_chess_v24_documentation.pdf"

echo "Creating PDF from RTF..."
echo ""

if [ ! -f "$RTF_FILE" ]; then
    echo "ERROR: RTF file not found: $RTF_FILE"
    echo "Run first: ./generate_docs.sh"
    exit 1
fi

# Method 1: Use cupsfilter (if available)
if command -v cupsfilter &> /dev/null; then
    echo "Using cupsfilter for conversion..."
    cupsfilter "$RTF_FILE" > "$PDF_OUTPUT" 2>/dev/null
    if [ -f "$PDF_OUTPUT" ] && [ -s "$PDF_OUTPUT" ]; then
        echo "✓ PDF created: $PDF_OUTPUT"
        exit 0
    fi
fi

# Method 2: Open in TextEdit and use print to PDF
echo "Opening RTF in TextEdit..."
echo "Steps:"
echo "1. TextEdit opens with the RTF file"
echo "2. Press Cmd+P (Print)"
echo "3. In the lower-left corner click 'PDF' -> 'Save as PDF'"
echo "4. Save as: $PDF_OUTPUT"
echo ""
echo "Opening TextEdit..."
open -a TextEdit "$RTF_FILE"

echo ""
echo "Alternatively you can use Microsoft Word:"
echo "  open -a 'Microsoft Word' $RTF_FILE"
echo "  (Then: File -> Save As -> PDF)"
echo ""
