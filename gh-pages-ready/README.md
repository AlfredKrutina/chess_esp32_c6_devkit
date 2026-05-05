# CZECHMATE — GitHub Pages (zdrojová složka)

Do větve **`gh-pages`** se při pushi na **`main`** / **`master`** automaticky nahrává **vygenerovaný** obsah: v CI se spustí Doxygen (`./generate_docs.sh`) a Mermaid (`./scripts/render_docs.sh`), výsledné HTML se složí do dočasné složky a nasadí ([`gh-pages.yml`](../.github/workflows/gh-pages.yml)). V repozitáři zde zůstávají tento `README`, **`downloads.html`** (marketing one-pager + stažení APK/DMG), složka **`landing/`** (CSS, JS, obrázky pro tuto stránku) a **`.nojekyll`**.

## Jednorázové nastavení na GitHubu

**Settings → Pages → Build and deployment:** zdroj **Deploy from a branch** → branch **`gh-pages`**, složka **`/` (root)** → Save.

Veřejná URL: `https://<uživatel>.github.io/<repo>/`

### 404 na `downloads.html` i když CI prošel

Deployment z [.github/workflows/gh-pages.yml](../.github/workflows/gh-pages.yml) zapisuje web na větev **`gh-pages`**. Pokud v **Settings → Pages** není zdroj **Branch: gh-pages** + **/(root)**, ale např. **main** + **/docs**, GitHub servuje **Jekyll** z `docs/` v main — soubor `downloads.html` tam není → **404**.

1. Ověř na GitHubu **Settings → Pages → Build and deployment → Source**.  
2. Nastav **Deploy from a branch** → **gh-pages** → **/ (root)**.  
3. Po propagaci otestuj `https://<uživatel>.github.io/<repo>/downloads.html`.

Tip: když v HTML kořene vidíš `<meta name="generator" content="Jekyll`, používáš špatný zdroj Pages.

## Ruční nasazení (bez Actions)

```bash
./generate_docs.sh
./scripts/render_docs.sh
rm -rf /tmp/czm-pages && mkdir -p /tmp/czm-pages
cp -a docs/doxygen/html/. /tmp/czm-pages/
cp -f docs/diagrams/diagrams_mermaid.html /tmp/czm-pages/diagrams_mermaid.html
cp -f gh-pages-ready/downloads.html /tmp/czm-pages/downloads.html
mkdir -p /tmp/czm-pages/landing
cp -a gh-pages-ready/landing/. /tmp/czm-pages/landing/
cp -f gh-pages-ready/.nojekyll /tmp/czm-pages/.nojekyll
git checkout --orphan gh-pages
git rm -rf . 2>/dev/null || true
cp -r /tmp/czm-pages/. .
git add .
git commit -m "Pages"
git push -u origin gh-pages --force
git checkout main
```

## 📁 Co je v `main` v této složce

- `.nojekyll` – zkopíruje se do kořene nasazeného webu (vypne Jekyll na GitHubu)
- `downloads.html` – veřejná produktová stránka + odkazy na GitHub Releases
- `landing/` – `landing.css`, `landing.js`, `assets/` (SVG placeholdery, `og-share.png` pro Open Graph; WebP z Blenderu přidávej sem a drž soubory pod cca desítky MB celkem)
- tento `README.md` – návod pro lidi; **žádné hromadné Doxygen HTML** (neplýtvá místem v gitu)

## 🔗 Odkazy

Po nasazení budou všechny odkazy fungovat:
- Hlavní stránka: `index.html`
- Mermaid diagramy: `diagrams_mermaid.html`
- Marketing + stažení aplikace: `downloads.html` (assety v `landing/`)

---

**Verze dokumentace:** 2.5.1 (sladit s kořenovým [README.md](../README.md))  
**Vygenerováno:** `./scripts/render_docs.sh` nebo `python3 generate_mermaid_html.py`; pak nahraj tento adresář na větev `gh-pages`.
