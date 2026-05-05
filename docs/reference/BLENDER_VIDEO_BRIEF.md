# CzechMate — brief pro videa z Blenderu (marketing / GitHub Pages)

Tento dokument popisuje **jednotlivá videa**, která mají vzniknout v Blenderu pro produktovou stránku [`downloads.html`](../../gh-pages-ready/downloads.html) a související prezentaci. Doplňuje statické exporty (WebP) popsané v komentáři v hlavičce téže stránky.

---

## Společné technické požadavky

| Parametr | Doporučení | Důvod |
|----------|------------|--------|
| **Formát pro self-host na Pages** | **WebM** (VP9 nebo AV1, pokud máš encoder) — sekundárně **H.264 MP4** v jednom souboru | GitHub Pages neomezuje typ MIME pro tyto přípony běžně; WebM je často menší než MP4 při stejné kvalitě. |
| **Cílová velikost souboru** | Ideálně **pod 8–12 MB** na jedno krátké video; **max. cca 15 MB** | Velké binárky zhoršují klon repa a načítání stránky; GitHub blokuje soubory **> 100 MB**. |
| **Rozlišení** | **1920×1080** nebo **1600×900** (16∶9) pro hero; sekční klipy mohou být **1280×720** | Stačí pro hero kontejner na webu; 4K do gitu nedávat. |
| **Snímky za sekundu** | **24** nebo **30** | Jednotně v projektu; 24 působí „filmověji“. |
| **Délka** | viz jednotlivá videa — typicky **6–20 s** | Krátké smyčky + fade pro bezševné loopování (`<video loop muted playsinline>`). |
| **Zvuk** | **Bez zvuku** (export bez audio tracku) nebo separátní hudba později | Autoplay v prohlížeči vyžaduje `muted`; jednodušší je čistě němé video. |
| **Barevný prostor / výstup** | **sRGB**, gamma podle „Standard“ předvolby pro web | Konzistence s WebP screenshoty. |
| **Smyčka** | První a poslední snímek **vizuálně sladit** (stejná pozice mlhy/světla) nebo použít **crossfade** ve strihu | HTML `loop` bez škoku. |
| **Alternativa k hostování v gitu** | **YouTube / Vimeo** (embed iframe) — bez limitu velikosti v repu | Pokud scéna vyžaduje vyšší bitrate nebo delší stopáž, je to preferovaná cesta (viz sekce „Distribuce“ níže). |

### Pojmenování souborů (navrhované)

Umístění po exportu: [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).

| Soubor | Obsah |
|--------|--------|
| `hero-loop.webm` | hlavní hero smyčka |
| `hero-loop.mp4` | fallback pro Safari / starší engine (volitelné, pokud `<video>` dostane dva `<source>`) |
| `board-detail-loop.webm` | detail desky / LED |
| `app-ui-loop.webm` | mock zařízení s UI (volitelné) |

Statické obrázky (ne video) drž zvlášť: `board-render.webp`, `app-mock.webp` podle [`downloads.html`](../../gh-pages-ready/downloads.html).

---

## Video 1 — Hero: „CzechMate v prostoru“

**Účel:** První dojem na [`downloads.html`](../../gh-pages-ready/downloads.html) — nahradit textový placeholder v bloku hero / nebo pozadí za titulkem (podle finální úpravy HTML).

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `hero-loop` |
| **Stopáž** | **10–16 s** na jednu smyčku |
| **Formát obrazu** | **16∶9** (1920×1080 nebo 1600×900) |
| **Obsah scény** | Celkový shot **fyzické šachovnice** (tvůj CAD / hrubý model desky): hrací pole, rámeček, náznak **tlačítkové řady**; volitelně velmi jemný **ESP modul** jako detail v pozadí (ne dominantní). |
| **Světlo / nálada** | Noční / studiová — **RGB akcent** v souladu s brandem (cyan–modrá, velmi jemný bloom na LED); **low-key**, čitelná silueta desky. |
| **Kamera** | Pomalý **orbit** kolem desky (15–30° oblouk) **nebo** jemný **dolly in** k centru pole; žádné agresivní handheld. |
| **LED animace** | Krátká **sekvence**: rozsvícení okrajů pole → jeden **highlight tahu** (např. e2→e4 abstraktně jako dvě zářící pole) → návrat do klidu; opakovatelné v smyčce. |
| **Post-processing** | Lehký **glare / bloom** jen na LED; **film grain** volitelně velmi slabý; nedělat přepálený HDR „hračkový“ look. |
| **Export** | WebM VP9, **2-pass** pokud možno; cíl **≤ 10 MB**. Volitelně druhý export H.264 pro Safari. |

**Storyboard (klíčové body):**

1. **0–3 s:** Široký záběr, deska částečně ve stínu, první LED „probudit“.
2. **3–8 s:** Kamera se přesune / rotuje; highlight tahu proběhne plynule.
3. **8–koniec:** Klidová pozice — musí **sedět** s framem 0 pro loop (nebo krátý fade out/in ve strihu).

---

## Video 2 — Detail hardware: „LED pole a hloubka“

**Účel:** Sekce **„LED a hardware“** — buď jako **loop v rámečku** vedle textu (alternativa k čistě statickému WebP), nebo krátký klip pod obrázkem.

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `board-detail-loop` |
| **Stopáž** | **8–12 s** |
| **Formát obrazu** | **16∶9** nebo **4∶3** (musí sedět s CSS `aspect-ratio` kontejneru na webu — aktuálně split používá ~4∶3) |
| **Obsah scény** | **Makro** na pole šachovnice: WS2812 buňky / difuzér, **řez modelem** nebo velmi mělká hloubka ostrosti (bokeh). |
| **Animace** | Pomalý **průběh barvy** po řádku/sloupci (scan); nebo **pulz** při „šach“ stavu jedním polem. |
| **Kamera** | Statická nebo mikro **pan** (pár mm ekvivalentu); žádný velký pohyb — čitelnost LED je prioritní. |
| **Export** | WebM, cíl **≤ 6–8 MB** (může být menší rozlišení než hero, např. 1280×720). |

**Storyboard:**

1. Detail pole bez světla → postupné **rozsvícení řady**.
2. Jedna buňka **přepne barvu** (tah / výstraha).
3. Návrat do klidového světelného vzoru → **match** na začátek smyčky.

---

## Video 3 — Aplikace v zařízení: „Mock glass UI“

**Účel:** Sekce **„Aplikace“** — místo nebo doplněk k [`app-mock.webp`](../../gh-pages-ready/landing/assets/app-placeholder.svg); ukázat **Flutter-like** rozhraní na telefonu/Mac okně.

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `app-ui-loop` |
| **Stopáž** | **12–18 s** |
| **Formát obrazu** | **16∶9** (model telefonu uprostřed letterboxu) nebo **9∶16** vyexportované do 16∶9 s tmavým pozadím |
| **Obsah scény** | Model **telefonu** (glass / rámeček); na display **jednoduchý shader** nebo **UV animovaná textura** s několika „obrazovkami“ aplikace (partie, připojení, nastavení). Nemusí být pixel-perfect s reálnou app — stačí **stejná barevná paleta** a typografie podobná Material 3. |
| **Animace** | Pomalý **scroll** nebo **crossfade** mezi 2–3 UI stavy; jemný **léč na zařízení** (reflex v rámečku). |
| **Kamera** | Velmi lehký **orbit** kolem telefonu nebo statika + pohyb UI uvnitř displeje. |
| **Export** | WebM, cíl **≤ 10 MB**. |

**Storyboard:**

1. Lock screen / splash **CzechMate**.
2. Obrazovka **připojení** (ikona BLE / Wi‑Fi abstraktně).
3. **Šachovnice** v app — jeden animovaný tah nebo zvýraznění pole.
4. Zpět na splash nebo klidová domovská obrazovka — **loop**.

---

## Video 4 (volitelné) — „Spojení deska ↔ telefon“

**Účel:** Krátký **konceptní** klip do budoucna (druhá stránka, sociální sítě, README GIF → lépe WebM).

| Položka | Specifikace |
|---------|-------------|
| **Pracovní název** | `sync-concept` |
| **Stopáž** | **6–10 s** |
| **Obsah** | **Split frame** nebo **grafické spojení** (partikly / linka) mezi modelem desky a telefonem; symbolika **datového toku** (ne nutně čitelný protokol). |
| **Styl** | Abstraktněji než Video 1–3; vhodné jako **teaser**. |
| **Export** | WebM **≤ 5 MB** nebo výhradně **YouTube**. |

---

## Distribuce na webu

### Varianta A — Soubor v repozitáři (aktuálně preferovaná pro krátké smyčky)

1. Exportuj WebM (± MP4) do [`gh-pages-ready/landing/assets/`](../../gh-pages-ready/landing/assets/).
2. V [`downloads.html`](../../gh-pages-ready/downloads.html) v hero sekci nahraď placeholder blokem:

```html
<video class="hero__video" data-hero-video autoplay muted loop playsinline>
  <source src="landing/assets/hero-loop.webm" type="video/webm">
  <source src="landing/assets/hero-loop.mp4" type="video/mp4">
</video>
```

3. Volitelně JavaScript v [`landing.js`](../../gh-pages-ready/landing/landing.js) už respektuje `prefers-reduced-motion` u `data-hero-video`.

### Varianta B — YouTube / Vimeo (delší nebo těžší klipy)

- Nahraj video jako **Neveřejné** nebo **Neuvedené v seznamu**.
- Do HTML vlož **iframe** (návod v komentáři v `downloads.html`).
- **Nevýhoda:** závislost na třetí straně; **výhoda:** žádná velikost v gitu, lepší CDN.

---

## Checklist před commitem

- [ ] Délka a velikost souboru v rozmezí výše  
- [ ] Smyčka bez rušivého skoku (nebo ořez ve strihu)  
- [ ] Bez audio tracku (nebo explicitně vypnutý v `<video>`)  
- [ ] Názvy souborů a cesty sedí s [`downloads.html`](../../gh-pages-ready/downloads.html)  
- [ ] Na širokém monitoru i mobilu **čitelný** hlavní motiv (test v prohlížeči po deployi Pages)  

---

## Související dokumentace

- Produktová stránka: [`gh-pages-ready/downloads.html`](../../gh-pages-ready/downloads.html)  
- Assety a ruční deploy: [`gh-pages-ready/README.md`](../../gh-pages-ready/README.md)  
- Mapa dokumentace: [`docs/README.md`](../README.md)  

Verze briefu: **1.0** — doplňovat podle reálných exportů z Blenderu (rozlišení, bitrate, finální názvy souborů).
