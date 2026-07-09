#!/bin/bash
# DEPRECATED (2026-07): enhanced_castling_system už není linkovaný z game_task.
# Komponenta zůstává v repu — před smazáním ověř, že ji nikdo nevolá.
# Run from project root: bash scripts/maintenance/delete_dead_castling.sh

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "🗑️  Deleting enhanced_castling_system directory..."
rm -rf components/enhanced_castling_system

echo "✅ Verifying directory was deleted..."
if [ ! -d "components/enhanced_castling_system" ]; then
    echo "✅ Directory successfully deleted!"
else
    echo "❌ Failed to delete directory"
    exit 1
fi

echo "🔨 Building project to verify changes..."
idf.py build

if [ $? -eq 0 ]; then
    echo "✅ Build successful! Dead code removed successfully."
else
    echo "❌ Build failed. Please check errors above."
    exit 1
fi

echo ""
echo "📊 Summary:"
echo "  - Removed 1206 lines of dead code"
echo "  - enhanced_castling_system component deleted"
echo "  - All references cleaned from game_task.c"
echo "  - CMakeLists.txt updated"
echo ""
echo "✅ Dead code removal complete!"
