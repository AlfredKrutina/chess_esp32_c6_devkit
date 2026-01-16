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
