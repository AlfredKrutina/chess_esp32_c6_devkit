# Jak nasdílet dokumentaci jako HTML

## Přehled možností

Máte několik možností, jak nasdílet HTML dokumentaci z Doxygenu:

1. **GitHub Pages** (nejjednodušší, zdarma)
2. **Web server na ESP32** (lokální přístup)
3. **Lokální HTTP server** (pro testování)
4. **Jiný hosting** (Netlify, Vercel, atd.)

## Možnost 1: GitHub Pages (DOPORUČENO)

### Výhody:
- ✅ Zdarma
- ✅ Automatická aktualizace při push
- ✅ Veřejně přístupné
- ✅ HTTPS

### Postup:

1. **Vytvořit branch `gh-pages`** (pokud ještě neexistuje):
```bash
git checkout -b gh-pages
```

2. **Zkopírovat HTML dokumentaci do rootu branchu**:
```bash
# Zkopírovat obsah docs/doxygen/html/ do rootu
cp -r docs/doxygen/html/* .
```

3. **Vytvořit `.nojekyll` soubor** (aby GitHub nespouštěl Jekyll):
```bash
touch .nojekyll
```

4. **Commit a push**:
```bash
git add .
git commit -m "Add Doxygen documentation"
git push origin gh-pages
```

5. **Aktivovat GitHub Pages**:
   - Jdi na GitHub → Settings → Pages
   - Source: `gh-pages` branch
   - Save

6. **Dokumentace bude dostupná na**:
   - `https://[username].github.io/[repo-name]/index.html`
   - Nebo `https://[username].github.io/[repo-name]/`

### Automatizace (volitelné):

Můžete vytvořit skript pro automatickou aktualizaci:

```bash
#!/bin/bash
# update_gh_pages.sh

# Generovat dokumentaci
./generate_docs.sh

# Přepnout na gh-pages branch
git checkout gh-pages

# Zkopírovat novou dokumentaci
cp -r docs/doxygen/html/* .
touch .nojekyll

# Commit a push
git add .
git commit -m "Update documentation $(date +%Y-%m-%d)"
git push origin gh-pages

# Vrátit se zpět na main branch
git checkout main
```

## Možnost 2: Web server na ESP32

### Výhody:
- ✅ Lokální přístup (bez internetu)
- ✅ Rychlé
- ✅ Integrované do projektu

### Postup:

1. **Přidat endpoint do web serveru** pro servování statických souborů z dokumentace

2. **Zkopírovat HTML soubory do SPIFFS nebo jako embedded data**

3. **Registrovat handler v `web_server_task.c`**:
```c
// Handler pro dokumentaci
httpd_uri_t docs_uri = {
    .uri = "/docs",
    .method = HTTP_GET,
    .handler = http_get_docs_handler,
    .user_ctx = NULL
};
httpd_register_uri_handler(httpd_handle, &docs_uri);
```

**Poznámka:** Toto vyžaduje úpravu kódu a může být náročné na paměť.

## Možnost 3: Lokální HTTP server

### Pro testování dokumentace lokálně:

#### Python (nejjednodušší):
```bash
cd docs/doxygen/html
python3 -m http.server 8000
```

Pak otevři v prohlížeči: `http://localhost:8000`

#### Node.js (http-server):
```bash
npm install -g http-server
cd docs/doxygen/html
http-server -p 8000
```

#### PHP:
```bash
cd docs/doxygen/html
php -S localhost:8000
```

## Možnost 4: Jiný hosting

### Netlify:
1. Zaregistruj se na netlify.com
2. Připoj GitHub repo
3. Build command: `./generate_docs.sh`
4. Publish directory: `docs/doxygen/html`
5. Deploy!

### Vercel:
1. Zaregistruj se na vercel.com
2. Připoj GitHub repo
3. Build command: `./generate_docs.sh`
4. Output directory: `docs/doxygen/html`

## Doporučení

**Pro veřejné sdílení:** GitHub Pages (Možnost 1)
- Nejsnadnější nastavení
- Automatická aktualizace
- Zdarma

**Pro lokální testování:** Python HTTP server (Možnost 3)
- Rychlé
- Žádná konfigurace

**Pro integraci do projektu:** Web server na ESP32 (Možnost 2)
- Vyžaduje úpravu kódu
- Může být náročné na paměť

## Aktualizace dokumentace

Po každé změně v kódu:

1. **Regenerovat dokumentaci**:
```bash
./generate_docs.sh
```

2. **Aktualizovat nasdílenou verzi** (podle zvolené možnosti):
   - GitHub Pages: push na `gh-pages` branch
   - Lokální server: restart serveru
   - ESP32: reflash firmware

## Struktura HTML dokumentace

Doxygen generuje:
- `index.html` - hlavní stránka
- `files.html` - seznam souborů
- `classes.html` - seznam tříd
- `functions.html` - seznam funkcí
- `*.html` - dokumentace jednotlivých souborů
- `*.js`, `*.css` - podpůrné soubory

**Důležité:** Všechny soubory musí být ve stejné složce, aby fungovaly odkazy!

## Tipy

1. **Zkontroluj, že všechny soubory jsou zahrnuty** - Doxygen generuje mnoho souborů, ujisti se, že jsou všechny zkopírovány

2. **Testuj lokálně před nasdílením** - použij lokální HTTP server

3. **Zkontroluj odkazy** - po nasdílení otevři dokumentaci a zkontroluj, že všechny odkazy fungují

4. **Aktualizuj README** - přidej odkaz na dokumentaci do README.md

