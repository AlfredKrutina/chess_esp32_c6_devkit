# CzechMate — co potřebuju z Blenderu na marketing / Pages

Tenhle text je spíš brief pro mě nebo pro někoho, kdo mi pomůže s videi na [`downloads.html`](../../gh-pages-ready/downloads.html). Doplňuje statické WebP exporty z komentáře v hlavičce té stránky.

---

## Společné technické požadavky

| Parametr | Co od toho chci | Proč |
|----------|-----------------|------|
| **Formát na self-host Pages** | **WebM** (VP9 nebo AV1 pokud encoder dává smysl), sekundárně **H.264 MP4** v jednom souboru | Pages běžně ty MIME types zvládne; WebM bývá menší než MP4 při podobné kvalitě. |
| **Velikost souboru** | ideálně **pod 8–12 MB** na krátký klip; **max. ~15 MB** | velké binárky zpomalují klon a načítání; GitHub hard stop je **100 MB**. |
| **Rozlišení** | **1920×1080** nebo **1600×900** (16∶9) pro hero; sekční klipy klidně **1280×720** | stačí na hero na webu; 4K do gitu necpat. |
| **FPS** | **24** nebo **30** | jednotně v projektu; 24 působí „filmověji“. |
| **Délka** | podle konkrétního videa — typicky **6–20 s** | krátká smyčka + fade pro `<video loop muted playsinline>`. |
| **Zvuk** | **bez audio stopy** (nebo hudba zvlášť později) | autoplay v prohlížeči stejně chce `muted`; jednodušší je němé video. |
| **Barevný prostor** | **sRGB**, gamma „standard“ pro web | sedí k WebP screenshotům. |
| **Smyčka** | první a poslední snímek **vizuálně sedí**, případně **crossfade** ve strihu | HTML `loop` bez škoku. |
| **Alternativa mimo git** | **YouTube / Vimeo** embed | když je scéna dlouhá nebo bitrate vysoký — nemusím cpát binárku do repa. |

### Pojmenování souborů (návrh)

Cílová složka po exportu: [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).

| Soubor | Obsah |
|--------|--------|
| `hero-loop.webm` | hlavní hero smyčka |
| `hero-loop.mp4` | fallback Safari / starší engine (volitelné druhé `<source>`) |
| `board-detail-loop.webm` | detail desky / LED |
| `app-ui-loop.webm` | mock zařízení s UI (volitelné) |

Statické obrázky držím zvlášť: `board-render.webp`, `app-mock.webp` podle [`downloads.html`](../../gh-pages-ready/downloads.html).

---

## Video 1 — Hero: „CzechMate v prostoru“

**Účel:** první dojem na [`downloads.html`](../../gh-pages-ready/downloads.html) — nahradit textový placeholder v hero / pozadí za titulkem.

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `hero-loop` |
| **Stopáž** | **10–16 s** na smyčku |
| **Formát obrazu** | **16∶9** (1920×1080 nebo 1600×900) |
| **Obsah** | celkový shot **fyzické šachovnice** (CAD / blok model): pole, rámeček, náznak **tlačítkové řady**; volitelně malý **ESP modul** v pozadí (ne dominantní). |
| **Světlo** | noční / studiová — **RGB akcent** v souladu s brandem (cyan–modrá, jemný bloom na LED); **low-key**, čitelná silueta desky. |
| **Kamera** | pomalý **orbit** (15–30°) **nebo** jemný **dolly in**; žádný agresivní handheld. |
| **LED** | krátká **sekvence**: okraj pole → **highlight tahu** (např. e2→e4 jako dvě zářící pole) → návrat do klidu; zacyklitelné. |
| **Post** | lehký **glare / bloom** jen na LED; **grain** jen hodně slabě. |
| **Export** | WebM VP9, ideálně **2-pass**, cíl **≤ 10 MB**; volitelně H.264 pro Safari. |

**Storyboard (hrubě):**

1. **0–3 s:** široký záběr, deska ve stínu, první LED „probudit“.
2. **3–8 s:** kamera se posune / otočí; highlight tahu.
3. **8–konec:** klidová pozice — **sedí** s framem 0 pro loop (nebo krátký fade).

---

## Video 2 — Detail hardware: „LED pole a hloubka“

**Účel:** sekce kolem hardware — loop vedle textu nebo pod statickým WebP.

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `board-detail-loop` |
| **Stopáž** | **8–12 s** |
| **Formát** | **16∶9** nebo **4∶3** (musí sedět s CSS kontejnerem na webu — split často ~4∶3) |
| **Obsah** | **makro** na pole: WS2812 / difuzér, řez modelem nebo mělká DOF. |
| **Animace** | pomalý **scan** barvy po řádku/sloupci nebo **pulz** „šach“ na jednom poli. |
| **Kamera** | statika nebo mikro **pan**. |
| **Export** | WebM, cíl **≤ 6–8 MB** (klidně 1280×720). |

**Storyboard:** tma → rozsvícení řady → změna jedné buňky → návrat do klidu → sedí s začátkem smyčky.

---

## Video 3 — Aplikace v zařízení: „Mock glass UI“

**Účel:** sekce **Aplikace** — doplněk k [`app-mock.webp`](../../gh-pages-ready/landing/assets/app-placeholder.svg).

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `app-ui-loop` |
| **Stopáž** | **12–18 s** |
| **Formát** | **16∶9** (telefon uprostřed letterboxu) nebo **9∶16** vložené do 16∶9 s tmavým pozadím |
| **Obsah** | model **telefonu**; na display jednoduchý shader / UV animovaná textura s několika „obrazovkami“ (partie, připojení, nastavení). Nemusí být pixel-perfect — stačí **podobná paleta** a Material-like typografie. |
| **Animace** | pomalý **scroll** nebo **crossfade** mezi 2–3 stavy; jemný **lesk** rámečku. |
| **Export** | WebM, cíl **≤ 10 MB**. |

**Storyboard:** splash CzechMate → připojení (BLE/Wi‑Fi abstraktně) → šachovnice v app → zpět na klid → loop.

---

## Video 4 (volitelné) — „Spojení deska ↔ telefon“

**Účel:** teaser na sociálně sítě / druhou stránku.

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `sync-concept` |
| **Stopáž** | **6–10 s** |
| **Obsah** | split frame nebo grafická „linka“ mezi deskou a telefonem — symbolika toku dat. |
| **Styl** | abstraktnější než V1–3. |
| **Export** | WebM **≤ 5 MB** nebo čistě **YouTube**. |

---

## Distribuce na webu

### Varianta A — soubor v repu (krátké smyčky)

1. WebM (± MP4) do [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).
2. V [`downloads.html`](../../gh-pages-ready/downloads.html) hero např.:

```html
<video class="hero__video" data-hero-video autoplay muted loop playsinline>
  <source src="landing/assets/hero-loop.webm" type="video/webm">
  <source src="landing/assets/hero-loop.mp4" type="video/mp4">
</video>
```

3. [`landing.js`](../../gh-pages-ready/landing/landing.js) už umí `prefers-reduced-motion` u `data-hero-video`.

### Varianta B — YouTube / Vimeo

- video jako **Neveřejné** / **Neuvedené v seznamu**
- iframe podle komentáře v `downloads.html`
- bez velkého binárního souboru v gitu

---

## Než to pošlu do gitu

- [ ] délka a velikost souboru v rozumném rozmezí  
- [ ] smyčka bez rušivého skoku  
- [ ] bez audio tracku (nebo vědomě vypnutý v `<video>`)  
- [ ] názvy souborů sedí s [`downloads.html`](../../gh-pages-ready/downloads.html)  
- [ ] na velkém monitoru i na mobilu je hlavní motiv čitelný  

---

## Související

- [`gh-pages-ready/downloads.html`](../../gh-pages-ready/downloads.html)  
- [`gh-pages-ready/README.md`](../../gh-pages-ready/README.md)  
- [`docs/README.md`](../README.md)  

Verze briefu: **1.0** — doplňuju podle reálných exportů (bitrate, finální názvy).
