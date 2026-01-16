#!/bin/bash
# Skript pro aktualizaci GitHub Pages s Doxygen dokumentací
# Tento skript zkopíruje vygenerovanou Doxygen HTML dokumentaci do gh-pages branch
# a nasadí ji na GitHub Pages

set -e  # Zastavit při chybě

# Barvy pro výstup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}📚 Aktualizace GitHub Pages dokumentace${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 1. Zkontrolovat, že jsme v git repozitáři
if [ ! -d .git ]; then
    echo -e "${RED}❌ Chyba: Nejsme v git repozitáři!${NC}"
    exit 1
fi

# 2. Zkontrolovat, že dokumentace existuje
if [ ! -d "docs/doxygen/html" ]; then
    echo -e "${YELLOW}⚠️  Dokumentace neexistuje, generuji...${NC}"
    if [ ! -f "./generate_docs.sh" ]; then
        echo -e "${RED}❌ Chyba: generate_docs.sh nenalezen!${NC}"
        exit 1
    fi
    ./generate_docs.sh
fi

# 3. Zkontrolovat, že dokumentace byla vygenerována
if [ ! -f "docs/doxygen/html/index.html" ]; then
    echo -e "${RED}❌ Chyba: Dokumentace nebyla vygenerována!${NC}"
    echo "Spusť: ./generate_docs.sh"
    exit 1
fi

# 3.1. Zkontrolovat, že diagrams_mermaid.html existuje
if [ ! -f "docs/doxygen/html/diagrams_mermaid.html" ]; then
    echo -e "${YELLOW}⚠️  Varování: diagrams_mermaid.html nenalezen v doxygen výstupu${NC}"
    echo "   Zkontrolujte, že Doxyfile obsahuje: HTML_EXTRA_FILES = docs/diagrams_mermaid.html"
else
    echo -e "${GREEN}✓ diagrams_mermaid.html nalezen${NC}"
fi

# 4. Uložit aktuální branch
CURRENT_BRANCH=$(git branch --show-current)
echo -e "${BLUE}📍 Aktuální branch: ${CURRENT_BRANCH}${NC}"

# 5. Zkontrolovat, zda existuje gh-pages branch
if git show-ref --verify --quiet refs/heads/gh-pages; then
    echo -e "${GREEN}✅ gh-pages branch existuje${NC}"
    git checkout gh-pages
    git pull origin gh-pages 2>/dev/null || true
else
    echo -e "${YELLOW}📝 Vytvářím nový gh-pages branch${NC}"
    git checkout --orphan gh-pages
    git rm -rf . 2>/dev/null || true
fi

# 6. Zkopírovat dokumentaci
echo -e "${BLUE}📋 Kopíruji dokumentaci...${NC}"
cp -r docs/doxygen/html/* .

# 6.1. Vytvořit .nojekyll (důležité pro GitHub Pages - zakáže Jekyll processing)
touch .nojekyll
echo -e "${GREEN}✓ Vytvořen .nojekyll soubor${NC}"

# 6.2. Zkontrolovat, že index.html existuje
if [ ! -f "index.html" ]; then
    echo -e "${RED}❌ Chyba: index.html nebyl zkopírován!${NC}"
    exit 1
fi

# 6.3. Zkontrolovat, že diagrams_mermaid.html existuje (pokud byl v doxygen výstupu)
if [ -f "diagrams_mermaid.html" ]; then
    echo -e "${GREEN}✓ diagrams_mermaid.html zkopírován${NC}"
fi

# 7. Commit a push
echo -e "${BLUE}💾 Commituji změny...${NC}"
git add .
if git diff --staged --quiet; then
    echo -e "${YELLOW}⚠️  Žádné změny k commitu${NC}"
else
    git commit -m "Update documentation $(date +%Y-%m-%d\ %H:%M:%S)" || true
    echo -e "${GREEN}✓ Změny zcommitovány${NC}"
fi

echo -e "${BLUE}🚀 Pushuji na GitHub...${NC}"
if git push origin gh-pages; then
    echo -e "${GREEN}✓ Push úspěšný${NC}"
else
    echo -e "${RED}❌ Chyba při push!${NC}"
    echo "   Zkontrolujte, že máte oprávnění push do gh-pages branch"
    echo "   Možná potřebujete: git push -u origin gh-pages"
fi

# 8. Vrátit se na původní branch
echo -e "${BLUE}↩️  Vracím se na branch: ${CURRENT_BRANCH}${NC}"
git checkout "$CURRENT_BRANCH"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✅ Hotovo!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${BLUE}📖 Dokumentace by měla být dostupná na:${NC}"
echo "   https://[username].github.io/[repo-name]/"
echo ""
echo -e "${YELLOW}💡 Důležité:${NC}"
echo "   1. Aktivujte GitHub Pages v Settings → Pages → Source: gh-pages branch"
echo "   2. Počkejte 1-2 minuty na nasazení"
echo "   3. Dokumentace bude dostupná na výše uvedené URL"
echo ""
echo -e "${BLUE}🔗 Odkazy v dokumentaci:${NC}"
echo "   - Hlavní stránka: index.html"
if [ -f "docs/doxygen/html/diagrams_mermaid.html" ]; then
    echo "   - Mermaid diagramy: diagrams_mermaid.html"
fi
echo ""

