# CzechMate na GitHub Pages — co tu mám připravené

Tahle složka (`gh-pages-ready/`) je zdroj toho, co pak GitHub nasadí jako veřejný web: **`downloads.html`** (produktovka + odkazy na appku), **`app_update.json`** (semver manifest pro kontrolu aktualizace Flutter klienta), **`app_update.html`** (lidsky čitelný popis schématu), **`landing/`** (styly, skripty, obrázky) a **`.nojekyll`**, aby GitHub náhodou nespouštěl Jekyll nad výstupem, který tam nepatří.

Když někdo pushne na **`main`** nebo **`master`**, Actions ([`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml)) mi z repa složí `_site`: přihodí se Doxygen HTML, Mermaid výstupy, tahle marketingová stránka a případně firmware metadata — a výsledek se zapíše na větev **`gh-pages`**.

## Jak to nastavím na GitHubu (jednou)

V **Settings → Pages → Build and deployment** nechám **Deploy from a branch**, větev **`gh-pages`**, složka **`/`** (root). Veřejná adresa pak vypadá jako `https://<uživatel>.github.io/<repo>/`.

### Když `downloads.html` hází 404 i přes úspěšný workflow

Deploy z CI jde na **`gh-pages`**. Když má Pages nastavené třeba **`main` + `/docs`**, GitHub mi servuje Jekyll z `docs/` a **`downloads.html` z tohoto workflow tam prostě není** → 404.

Co dělám: zkontroluju zdroj Pages (má to být **gh-pages / root**). Když ve zdrojáku úvodní stránky vidím `<meta name="generator" content="Jekyll"`, běžím ze špatného zdroje.

### Lokální náhled `downloads.html` (hero video)

YouTube embed v hero sekci **nefunguje při `file://`** (typicky Error 153). Spusť z této složky HTTP server a otevři URL v prohlížeči:

```bash
cd gh-pages-ready && ./serve.sh
```

Pak např. `http://127.0.0.1:8765/downloads.html`. Jiný port: `./serve.sh 9000`.

## Ruční nasazení (když nechci čekat na Actions)

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

## Co přesně tu v main větvi leží

- **`.nojekyll`** — zkopíruje se do kořene nasazeného webu.
- **`app_update.json`** — manifest verze aplikace (`latest_version`, volitelně `min_supported_version`, `release_page_url`). Klient: `flutter_czechmate/lib/core/constants/app_update_defaults.dart`. Po release aplikace srovnej s `flutter_czechmate/pubspec.yaml`. **Důležité:** veřejná adresa vede na nasazenou větev **gh-pages**; soubor se u uživatelů změní až po **úspěšném deployi** workflow (typicky push na `main` se změnou tohoto JSONu nebo jiné sledované cesty — případně ruční spuštění *workflow_dispatch* na stejném commitu znovu nasadí aktuální obsah repa). Samotná úprava jen ve Flutter kódu bez commitu manifestu deploy neaktualizuje.
- **`app_update.html`** — stručná dokumentace schématu pro maintainery.
- **`downloads.html`** — veřejná stránka ke stažení APK / DMG / Windows installer + texty kolem CzechMate. **V2** (Hall, komerce) vs **V1** (reed, video, fw **1.8.0**) je vysvětlené přímo na stránce; dotazníky a předobjednávka = **V2**. Sekce **Aplikace**: karty otevírají modal — obsah v `landing/landing.js` (`FEATURE_PAGES`). Tlačítka stažení mají fallback na stránku release; `landing.js` dotáhne z GitHub API `releases/latest` přímé `browser_download_url` pro `.apk`, `.dmg` a soubor končící na `windows-setup.exe`.
- **`landing/`** — `landing.css`, `landing.js`, `assets/` (WebP, SVG, OG obrázek). Specifikace médií a video integrace: **`context/MEDIA_DELIVERABLES.md`**.
- **`README.md`** — tenhle soubor (spíš pro mě než pro návštěvníka webu).

Žádné megabyte Doxygen HTML sem do gitu netahám — generuje se v CI / lokálně.

## Sekce „Zájem“ na `downloads.html` a Google Formuláře

Dvě oddělené věci — na stránce jsou jen **odkazy** (žádný vlastní POST z JS):

1. **Předobjednávka** (jméno, kontakt, přání k produktu, …) — veřejný odkaz pro respondenty:
   `https://docs.google.com/forms/d/18ns5uSUSzr5zcHsiZwD1HWfY15xBa-folmE-oH86BsY/viewform`  
   *(v editoru Google Forms je stejný formulář pod URL končící na `/edit` — na web dávám vždy `/viewform`.)*

2. **Průzkum zájmu / hodnocení** — samostatný dotazník:
   `https://docs.google.com/forms/d/e/1FAIpQLSck_q6sjN1nnUs9aV2CsY0MyPNo9puLcncW603iEJz6BMLjPw/viewform`

Odpovědi žijí v Google účtu (tabulky, upozornění); deploy nové verze statické stránky je **nesmaže**.

Automatické soukromé DM na Instagram přímo z čistě statického webu bez vlastního serveru a Meta schválení je nespolehlivé — realita je spíš formulář → tabulka / mail → případně Make, Zapier nebo vlastní skript.

**Kde to sedí v HTML:** v levém panelu sekce Zájem je odkaz na průzkum zájmu; tlačítko „Předobjednat“ otevře modal s odkazem na předobjednávkový formulář.

## Odkazy po nasazení

- `index.html` — dokumentace (z Doxygen workflow).
- `diagrams_mermaid.html` — sekvenční diagramy.
- `downloads.html` — marketing + stažení aplikace + sekce zájmu.
- `app_update.json` — strojová kontrola aktualizace aplikace.
- `app_update.html` — popis manifestu.

Video na stránce / na YouTube: [youtu.be/_MS6OP3x6Z4](https://youtu.be/_MS6OP3x6Z4). Na **HTTPS** se embed načítá spolehlivěji; u `file://` často zůstane jen náhled kvůli omezením přehrávače.

---

**Verze textů:** sladím s kořenovým [README.md](../README.md).  
Diagramová část se regeneruje přes `./scripts/render_docs.sh` nebo `python3 generate_mermaid_html.py` podle toho, co zrovna měním.

Push na `main`, který mění soubory podle filtru v [`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml), nasadí tuto složku na větev `gh-pages`. Flutter APK/DMG/EXE z CI žije na GitHub Releases — sekce stažení na `downloads.html` jen odkazuje přes API `releases/latest`.
