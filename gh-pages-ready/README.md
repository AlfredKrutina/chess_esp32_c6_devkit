# CZECHMATE — GitHub Pages (zdrojová složka)

Obsah této složky se při pushi na **`main`** / **`master`** automaticky nasadí na větev **`gh-pages`** ([workflow `gh-pages.yml`](../.github/workflows/gh-pages.yml)).

## Jednorázové nastavení na GitHubu

**Settings → Pages → Build and deployment:** zdroj **Deploy from a branch** → branch **`gh-pages`**, složka **`/` (root)** → Save.

Veřejná URL: `https://<uživatel>.github.io/<repo>/`

## Ruční nasazení (bez Actions)

```bash
git checkout --orphan gh-pages
git rm -rf . 2>/dev/null || true
cp -r gh-pages-ready/. .
git add .
git commit -m "Pages"
git push -u origin gh-pages --force
git checkout main
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

**Verze dokumentace:** 2.5.1 (sladit s kořenovým [README.md](../README.md))  
**Vygenerováno:** `./scripts/render_docs.sh` nebo `python3 generate_mermaid_html.py`; pak nahraj tento adresář na větev `gh-pages`.
