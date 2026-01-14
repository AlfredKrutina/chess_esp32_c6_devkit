#!/bin/bash
# Skript pro aktualizaci GitHub Pages s Doxygen dokumentací

set -e  # Zastavit při chybě

echo "📚 Aktualizace GitHub Pages dokumentace..."

# 1. Zkontrolovat, že jsme v git repozitáři
if [ ! -d .git ]; then
    echo "❌ Chyba: Nejsme v git repozitáři!"
    exit 1
fi

# 2. Zkontrolovat, že dokumentace existuje
if [ ! -d "docs/doxygen/html" ]; then
    echo "⚠️  Dokumentace neexistuje, generuji..."
    ./generate_docs.sh
fi

# 3. Zkontrolovat, že dokumentace byla vygenerována
if [ ! -f "docs/doxygen/html/index.html" ]; then
    echo "❌ Chyba: Dokumentace nebyla vygenerována!"
    echo "Spusť: ./generate_docs.sh"
    exit 1
fi

# 4. Uložit aktuální branch
CURRENT_BRANCH=$(git branch --show-current)
echo "📍 Aktuální branch: $CURRENT_BRANCH"

# 5. Zkontrolovat, zda existuje gh-pages branch
if git show-ref --verify --quiet refs/heads/gh-pages; then
    echo "✅ gh-pages branch existuje"
    git checkout gh-pages
    git pull origin gh-pages 2>/dev/null || true
else
    echo "📝 Vytvářím nový gh-pages branch"
    git checkout --orphan gh-pages
    git rm -rf . 2>/dev/null || true
fi

# 6. Zkopírovat dokumentaci
echo "📋 Kopíruji dokumentaci..."
cp -r docs/doxygen/html/* .
touch .nojekyll

# 7. Commit a push
echo "💾 Commituji změny..."
git add .
git commit -m "Update documentation $(date +%Y-%m-%d\ %H:%M:%S)" || echo "Žádné změny k commitu"

echo "🚀 Pushuji na GitHub..."
git push origin gh-pages

# 8. Vrátit se na původní branch
echo "↩️  Vracím se na branch: $CURRENT_BRANCH"
git checkout "$CURRENT_BRANCH"

echo "✅ Hotovo! Dokumentace by měla být dostupná na:"
echo "   https://[username].github.io/[repo-name]/"
echo ""
echo "💡 Nezapomeň aktivovat GitHub Pages v Settings → Pages!"

