# CzechMate na GitHub Pages — co tu mám připravené

Tahle složka (`gh-pages-ready/`) je zdroj toho, co pak GitHub nasadí jako veřejný web: **`downloads.html`** (produktovka + odkazy na appku), **`landing/`** (styly, skripty, obrázky) a **`.nojekyll`**, aby GitHub náhodou nespouštěl Jekyll nad výstupem, který tam nepatří.

Když někdo pushne na **`main`** nebo **`master`**, Actions ([`.github/workflows/gh-pages.yml`](../.github/workflows/gh-pages.yml)) mi z repa složí `_site`: přihodí se Doxygen HTML, Mermaid výstupy, tahle marketingová stránka a případně firmware metadata — a výsledek se zapíše na větev **`gh-pages`**.

## Jak to nastavím na GitHubu (jednou)

V **Settings → Pages → Build and deployment** nechám **Deploy from a branch**, větev **`gh-pages`**, složka **`/`** (root). Veřejná adresa pak vypadá jako `https://<uživatel>.github.io/<repo>/`.

### Když `downloads.html` hází 404 i přes úspěšný workflow

Deploy z CI jde na **`gh-pages`**. Když má Pages nastavené třeba **`main` + `/docs`**, GitHub mi servuje Jekyll z `docs/` a **`downloads.html` z tohoto workflow tam prostě není** → 404.

Co dělám: zkontroluju zdroj Pages (má to být **gh-pages / root**). Když ve zdrojáku úvodní stránky vidím `<meta name="generator" content="Jekyll"`, běžím ze špatného zdroje.

## Ruční nasazení (když nechci čekat na Actions)

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

## Co přesně tu v main větvi leží

- **`.nojekyll`** — zkopíruje se do kořene nasazeného webu.
- **`downloads.html`** — veřejná stránka ke stažení APK/DMG + texty kolem CzechMate.
- **`landing/`** — `landing.css`, `landing.js`, `assets/` (OG obrázek, placeholdery; větší WebP z Blenderu držím rozumně malé).
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

Video na stránce / na YouTube: [youtu.be/_MS6OP3x6Z4](https://youtu.be/_MS6OP3x6Z4). Na **HTTPS** se embed načítá spolehlivěji; u `file://` často zůstane jen náhled kvůli omezením přehrávače.

---

**Verze textů:** sladím s kořenovým [README.md](../README.md).  
Diagramová část se regeneruje přes `./scripts/render_docs.sh` nebo `python3 generate_mermaid_html.py` podle toho, co zrovna měním.
