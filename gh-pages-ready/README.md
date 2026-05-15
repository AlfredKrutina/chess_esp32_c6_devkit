# CzechMate — GitHub Pages (`gh-pages-ready/`)

Složka obsahuje statické soubory pro veřejný web: **`downloads.html`** (produktová stránka a odkazy na aplikaci), **`app_update.json`** (manifest semver pro kontrolu aktualizace ve Flutter klientovi), **`app_update.html`** (popis schématu JSON), **`landing/`** (CSS, JavaScript, obrázky a videa) a **`.nojekyll`**, aby GitHub Pages nespouštěl Jekyll nad tímto výstupem.

Při pushi na **`main`** nebo **`master`**, pokud commit mění některou z cest uvedených ve workflow (viz níže), spustí se [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml), složí `_site` (Doxygen, Mermaid, tato stránka, případně firmware `.bin`) a zapíše výsledek na větev **`gh-pages`**. Změny jen ve větvích mimo `main`/`master` deploy nespuští (kromě ručního **workflow_dispatch** v Actions).

## GitHub Pages — jednorázové nastavení

V **Settings → Pages → Build and deployment**: zdroj **Deploy from a branch**, větev **`gh-pages`**, složka **`/`** (root). Veřejná adresa má tvar `https://<uživatel>.github.io/<repo>/`.

### `downloads.html` vrací 404

Kontrola: Pages musí číst z větve **`gh-pages`**, ne z **`main`** + **`/docs`**. Při špatném zdroji stránka vůbec neobsahuje nasazený obsah z workflow. V HTML zdroji úvodní stránky výskyt `<meta name="generator" content="Jekyll">` značí, že se servuje jiný kořen než artefakt z tohoto repa.

## Lokální náhled `downloads.html`

YouTube v hero vyžaduje HTTP(S). Protokol **`file://`** často vypne embed; `landing.js` v tom případě nahradí oblast statickým obrázkem z YouTube CDN.

Relativní cesty `landing/assets/…` jsou platné vůči **kořeni dokumentů = `gh-pages-ready`**. HTTP server musí mít document root tuto složku; jinak média vrátí **404** a u `<video>` zůstane jen poster.

- **Doporučeno:** `cd gh-pages-ready` → `serve.cmd` / `serve.ps1` / `./serve.sh` → v prohlížeči `http://127.0.0.1:8765/downloads.html`
- **Server z kořene repozitáře:** otevřít `http://127.0.0.1:<port>/gh-pages-ready/downloads.html`, nebo z kořene spustit **`serve-downloads.cmd`**.

**Windows (PowerShell / CMD, adresář `gh-pages-ready`):**

```powershell
.\serve.cmd
```

Volitelný port: `.\serve.cmd 9000`. Při zablokovaném `serve.ps1` kvůli execution policy použít `serve.cmd` nebo přímo `python -m http.server 8765` ve stejné složce.

**Git Bash / WSL / Linux / macOS:**

```bash
cd gh-pages-ready && ./serve.sh
```

## Ruční nasazení (bez čekání na Actions)

```bash
./generate_docs.sh
./scripts/render_docs.sh
rm -rf /tmp/czm-pages && mkdir -p /tmp/czm-pages
cp -a docs/doxygen/html/. /tmp/czm-pages/
cp -f docs/diagrams/diagrams_mermaid.html /tmp/czm-pages/diagrams_mermaid.html
cp -f gh-pages-ready/downloads.html /tmp/czm-pages/downloads.html
cp -f gh-pages-ready/app_update.json /tmp/czm-pages/app_update.json
cp -f gh-pages-ready/app_update.html /tmp/czm-pages/app_update.html
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

## Obsah složky (větev `main`)

| Soubor / složka | Účel |
|-----------------|------|
| `.nojekyll` | Zkopíruje se do kořene nasazeného webu. |
| `app_update.json` | `latest_version`, volitelně `min_supported_version`, `release_page_url`. Klient: `flutter_czechmate/lib/core/constants/app_update_defaults.dart`. Po vydání aplikace srovnat s `flutter_czechmate/pubspec.yaml`. Změna se projeví u uživatelů až po deployi na **`gh-pages`**. |
| `app_update.html` | Lidsky čitelný popis schématu JSON. |
| `downloads.html` | Produktová stránka, stažení APK/DMG/Windows installeru, sekce zájmu. Rozdíly V1/V2 a formuláře jsou popsány přímo na stránce. Modaly funkcí: `landing/landing.js` (`FEATURE_PAGES`). Odkazy stažení: výchozí cíl je stránka release; `landing.js` doplňuje přímé `browser_download_url` z GitHub API `releases/latest` pro `.apk`, `.dmg` a `windows-setup.exe`. |
| `landing/` | `landing.css`, `landing.js`, `assets/` (SVG, WebP, MP4, OG obrázek). |

## Média (MP4)

- Hero: vložený YouTube (`loading="lazy"`, `fetchpriority="low"`, `preconnect` na `youtube.com` a `i.ytimg.com`).
- Sekce deska: `landing/assets/czm-v2-led-loop.mp4`.
- Sekce aplikace: `landing/assets/czm-v2-app-iphone.mp4` v rámu `split__media` + `split__video` (poměr 4:3, `object-fit: cover`). Přehrávání **bez autoplay** — start když je video **dostatečně vidět ve viewportu** (`data-play-when-visible` + `initAppDemoVideoPlayWhenVisible` v `landing.js`, Intersection Observer); při `prefers-reduced-motion: reduce` zůstane zastavené.
- Obě lokální MP4: `preload="none"`, zdroj přes `data-src` a `data-lazy-local` + `IntersectionObserver` v `landing.js` (načtení před vstupem do viewportu; start stahů dvou souborů je rozestřený o ~220 ms). Nativní ovládací panel videa se zapne až po **kliknutí na video** (`split__video--controls-on-click` v `landing.js`).
- Sekce AI kouč: obrázek `#ai-kouc-app-preview` se generuje z téhož MP4 v `landing.js` (canvas → JPEG) v čase daném atributem **`data-ai-preview-at`** na `#app-phone-demo-video` (desetinný podíl délky, výchozí **0.52** — konec souboru často černý nebo prázdný). Rozvržení: `split__media--ai-preview` + `split__media--ai-preview__pan` (středová část šířky, **plná výška snímku**, bez svislého ořezu).

Obecná konverze (včetně zvuku):

```bash
ffmpeg -y -i vstup.mp4 -c:v libx264 -profile:v high -pix_fmt yuv420p -movflags +faststart -c:a aac -b:a 128k vystup.mp4
```

**Lehčí MP4 na web** (doporučeno pro smyčky na `downloads.html`: menší rozlišení, CRF 26–28, `faststart`; u němých klipů bez zvuku přidej `-an`):

```bash
ffmpeg -y -i vstup.mp4 -an -vf "scale=min(1280\,iw):-2" -c:v libx264 -preset medium -crf 26 -profile:v high -pix_fmt yuv420p -movflags +faststart vystup-web.mp4
```

## Sekce „Zájem“ a Google Formuláře

Stránka používá pouze **odkazy** (žádný vlastní POST z JavaScriptu).

1. **Předobjednávka:** `https://docs.google.com/forms/d/18ns5uSUSzr5zcHsiZwD1HWfY15xBa-folmE-oH86BsY/viewform` (respondenti: `/viewform`; v editoru Forms stejný formulář pod `/edit`).
2. **Průzkum zájmu:** `https://docs.google.com/forms/d/e/1FAIpQLSck_q6sjN1nnUs9aV2CsY0MyPNo9puLcncW603iEJz6BMLjPw/viewform`

Odpovědi zůstávají v Google účtu vlastníka formulářů; deploy nové verze statické stránky je nemění.

**Umístění v HTML:** v sekci Zájem odkaz na průzkum v levém panelu; tlačítko předobjednávky otevírá modal s odkazem na předobjednávkový formulář.

## Odkazy po nasazení

- `index.html` — technická dokumentace (z CI).
- `diagrams_mermaid.html` — diagramy.
- `downloads.html` — marketing a stažení.
- `app_update.json` — strojová kontrola verze aplikace.
- `app_update.html` — dokumentace manifestu.

YouTube (stejné ID jako v hero): [youtu.be/_MS6OP3x6Z4](https://youtu.be/_MS6OP3x6Z4).

---

Koordinace s kořenovým [README.md](../README.md). Diagramy: `./scripts/render_docs.sh` nebo `python3 generate_mermaid_html.py` podle workflow v repozitáři.

Push na `main` nebo `master`, který mění alespoň jednu z cest v [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml) (včetně celé složky `gh-pages-ready/**`), znovu nasadí obsah na větev `gh-pages`. Binárky aplikace (APK, DMG, EXE) jsou na GitHub Releases; sekce stažení na `downloads.html` je s nimi propojená přes API `releases/latest`. Ruční spuštění: Actions → „Deploy GitHub Pages“ → **Run workflow**.
