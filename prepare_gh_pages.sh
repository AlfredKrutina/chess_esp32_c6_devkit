#!/bin/bash
# Skript pro přípravu dokumentace pro GitHub Pages (bez git operací)
# Tento skript připraví dokumentaci do složky, kterou pak můžete nahrát na GitHub Pages

set -e  # Zastavit při chybě

# Barvy pro výstup
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}📚 Příprava dokumentace pro GitHub Pages${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 1. Zkontrolovat, že dokumentace existuje
if [ ! -d "docs/doxygen/html" ]; then
    echo -e "${YELLOW}⚠️  Dokumentace neexistuje, generuji...${NC}"
    if [ ! -f "./generate_docs.sh" ]; then
        echo -e "${RED}❌ Chyba: generate_docs.sh nenalezen!${NC}"
        exit 1
    fi
    ./generate_docs.sh
fi

# 2. Zkontrolovat, že dokumentace byla vygenerována
if [ ! -f "docs/doxygen/html/index.html" ]; then
    echo -e "${RED}❌ Chyba: Dokumentace nebyla vygenerována!${NC}"
    echo "Spusť: ./generate_docs.sh"
    exit 1
fi

# 3. Zkontrolovat, že diagrams_mermaid.html existuje
if [ ! -f "docs/doxygen/html/diagrams_mermaid.html" ]; then
    echo -e "${YELLOW}⚠️  Varování: diagrams_mermaid.html nenalezen v doxygen výstupu${NC}"
    echo "   Zkontrolujte, že Doxyfile obsahuje: HTML_EXTRA_FILES = docs/diagrams_mermaid.html"
else
    echo -e "${GREEN}✓ diagrams_mermaid.html nalezen${NC}"
fi

# 4. Vytvořit výstupní složku
OUTPUT_DIR="gh-pages-ready"
echo -e "${BLUE}📋 Připravuji dokumentaci v složce: ${OUTPUT_DIR}${NC}"

# 5. Vymazat starou složku (pokud existuje)
if [ -d "$OUTPUT_DIR" ]; then
    echo -e "${YELLOW}⚠️  Složka ${OUTPUT_DIR} již existuje, mažu...${NC}"
    rm -rf "$OUTPUT_DIR"
fi

# 6. Vytvořit novou složku
mkdir -p "$OUTPUT_DIR"

# 7. Zkopírovat dokumentaci
echo -e "${BLUE}📋 Kopíruji dokumentaci...${NC}"
cp -r docs/doxygen/html/* "$OUTPUT_DIR/"

# 8. Vytvořit .nojekyll (důležité pro GitHub Pages - zakáže Jekyll processing)
touch "$OUTPUT_DIR/.nojekyll"
echo -e "${GREEN}✓ Vytvořen .nojekyll soubor${NC}"

# 9. Zkontrolovat, že index.html existuje
if [ ! -f "$OUTPUT_DIR/index.html" ]; then
    echo -e "${RED}❌ Chyba: index.html nebyl zkopírován!${NC}"
    exit 1
fi

# 10. Zkontrolovat, že diagrams_mermaid.html existuje (pokud byl v doxygen výstupu)
if [ -f "$OUTPUT_DIR/diagrams_mermaid.html" ]; then
    echo -e "${GREEN}✓ diagrams_mermaid.html zkopírován${NC}"
fi

# 11. Vytvořit README s instrukcemi
cat > "$OUTPUT_DIR/README.md" << 'EOF'
# CZECHMATE - Dokumentace pro GitHub Pages

Tato složka obsahuje připravenou dokumentaci pro GitHub Pages.

## 📖 Jak nahrát na GitHub Pages

### Metoda 1: Přes GitHub Web UI

1. Vytvořte nový repozitář na GitHubu (nebo použijte existující)
2. V repozitáři vytvořte nový branch s názvem `gh-pages`:
   - GitHub → Branches → New branch → název: `gh-pages`
3. Nahrajte obsah této složky do root gh-pages branch:
   - GitHub → Upload files → vyberte všechny soubory z této složky
4. Commit změny
5. Aktivujte GitHub Pages:
   - Settings → Pages → Source: `gh-pages` branch → Save
6. Počkejte 1-2 minuty na nasazení
7. Dokumentace bude dostupná na: `https://[username].github.io/[repo-name]/`

### Metoda 2: Přes Git (pokud máte git repo)

```bash
# Vytvořte gh-pages branch
git checkout --orphan gh-pages
git rm -rf . 2>/dev/null || true

# Zkopírujte obsah této složky
cp -r gh-pages-ready/* .

# Commit a push
git add .
git commit -m "Add documentation"
git push -u origin gh-pages

# Vraťte se na hlavní branch
git checkout main  # nebo master
```

### Metoda 3: Přes GitHub CLI (gh)

```bash
# Pokud máte GitHub CLI nainstalovaný
cd gh-pages-ready
gh repo create [repo-name] --public --source=. --remote=origin
git push -u origin gh-pages
```

## 📁 Struktura

- `index.html` - Hlavní stránka dokumentace
- `diagrams_mermaid.html` - Mermaid diagramy
- `.nojekyll` - Zakáže Jekyll processing (důležité!)
- Všechny další HTML, CSS, JS soubory potřebné pro dokumentaci

## 🔗 Odkazy

Po nasazení budou všechny odkazy fungovat:
- Hlavní stránka: `index.html`
- Mermaid diagramy: `diagrams_mermaid.html`

---

**Verze dokumentace:** 2.4.0  
**Vygenerováno:** Automaticky pomocí prepare_gh_pages.sh
EOF

echo -e "${GREEN}✓ Vytvořen README.md s instrukcemi${NC}"

# 12. Zobrazit souhrn
echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✅ Hotovo!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${BLUE}📁 Dokumentace je připravena v složce: ${OUTPUT_DIR}${NC}"
echo ""
echo -e "${BLUE}📖 Další kroky:${NC}"
echo "   1. Otevřete složku: ${OUTPUT_DIR}"
echo "   2. Postupujte podle instrukcí v ${OUTPUT_DIR}/README.md"
echo ""
echo -e "${BLUE}🔗 Možnosti nasazení:${NC}"
echo "   - GitHub Web UI (nejjednodušší)"
echo "   - Git (pokud máte git repo)"
echo "   - GitHub CLI (pokud máte gh nainstalovaný)"
echo ""
echo -e "${YELLOW}💡 Tip:${NC}"
echo "   Pokud chcete použít automatický skript s git operacemi,"
echo "   použijte: ./update_gh_pages.sh (vyžaduje git repo)"
echo ""
