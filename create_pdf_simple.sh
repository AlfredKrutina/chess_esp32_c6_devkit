#!/bin/bash
# Jednoduchý skript pro vytvoření PDF z RTF pomocí macOS
# ESP32-C6 Chess v2.4

RTF_FILE="docs/doxygen/rtf/refman.rtf"
PDF_OUTPUT="docs/doxygen/esp32_chess_v24_documentation.pdf"

echo "Vytváření PDF z RTF..."
echo ""

if [ ! -f "$RTF_FILE" ]; then
    echo "CHYBA: RTF soubor nenalezen: $RTF_FILE"
    echo "Nejprve spusťte: ./generate_docs.sh"
    exit 1
fi

# Metoda 1: Použít cupsfilter (pokud je dostupný)
if command -v cupsfilter &> /dev/null; then
    echo "Použití cupsfilter pro konverzi..."
    cupsfilter "$RTF_FILE" > "$PDF_OUTPUT" 2>/dev/null
    if [ -f "$PDF_OUTPUT" ] && [ -s "$PDF_OUTPUT" ]; then
        echo "✓ PDF vytvořen: $PDF_OUTPUT"
        exit 0
    fi
fi

# Metoda 2: Otevřít v TextEdit a použít tisk do PDF
echo "Otevření RTF v TextEdit..."
echo "Postup:"
echo "1. TextEdit se otevře s RTF souborem"
echo "2. Stiskněte Cmd+P (Tisk)"
echo "3. V levém dolním rohu klikněte na 'PDF' -> 'Uložit jako PDF'"
echo "4. Uložte jako: $PDF_OUTPUT"
echo ""
echo "Otevírám TextEdit..."
open -a TextEdit "$RTF_FILE"

echo ""
echo "Alternativně můžete použít Microsoft Word:"
echo "  open -a 'Microsoft Word' $RTF_FILE"
echo "  (Pak: Soubor -> Uložit jako -> PDF)"
echo ""

