# CZECHMATE — komplexní plán rozšíření (UI, analýza, puzzle, statistiky, ekosystém)

Dokument slouží jako **dlouhodobý backlog** a architektonický rámec. Část položek je už částečně implementována (viz kód); zbytek je návrh k iterativnímu doručování.

---

## 1. Vize

**CZECHMATE** má spojit **fyzickou šachovnici (ESP)** s **nativním iOS zážitkem** na úrovni „App Store“ (přehlednost, důvěra, rychlost), bez nutnosti webového prohlížeče pro běžnou hru. Doplňkově nabídnout **analýzu**, **puzzle ekosystém** a **statistiky** srovnatelné s populárními aplikacemi — s respektem k tomu, že jádro pravdy zůstává na firmware / LAN API.

---

## 2. UI a rozvržení (Hra a globální design)

| Stav | Úkol |
|------|------|
| Částečně hotovo | Centrální `Theme.swift`, karty, primární tlačítka, Hra jako šachovnice + kompaktní hodiny + `DisclosureGroup` pro pokročilé ovládání. |
| Další kroky | **Responzivní rozvržení** na iPad (split: deska \| postranní panel). **Témata** (světlé / tmavé / OLED). **Přístupnost**: Dynamic Type, VoiceOver popisy polí. **Animace** při tahu (volitelně vypnutelné). |
| Nápad | **Režim „jen deska“** (fullscreen gestem), skrytí všech panelů kromě hodin. |

---

## 3. Live Activity, Dynamic Island a Apple Watch

| Stav | Úkol |
|------|------|
| **Hotovo (aktuální iterace)** | `MaterialEvaluator` ve sdíleném balíčku — text **kdo je materiálem navrch** („Navrch bílý (+2)“ …). Live Activity zobrazuje **advantageSummary**, při `timerActive` i **obě hodiny**; vylepšený widget rozhraní. WatchConnectivity posílá `advantageSummary` + `clockSyncAt`; na **Watch** přehled (`glance`) ukazuje materiál, **živý odpočet** aktivního hráče přes `TimelineView` (stejná logika jako na iPhone). |
| Další kroky | **Častější aktualizace** Live Activity při čistě lokálním odpočtu (bez nového snapshotu) — buď lehčí throttling v `BoardCompanionSync`, nebo **App Intents** / krátký timer na pozadí (s ohledem na baterii). **Komplikace na Watch face** (časy + kdo táhne). **Chybové stavy**: když spojení padne, jednotná hláška na Watch i v aktivitě. |
| Riziko | Push aktualizací z ESP bez iPhonu uprostřed — Watch vždy závislá na iOS jako mostu. |

---

## 4. Analýza — směr „jako chess.com“ (postupně)

Chess.com v analýze kombinuje **více variant motoru**, **eval graf**, **knihovnu zahájení**, **accuracy** — u nás je zdroj pravdy často **Stockfish přes cloud API** + lokální FEN.

| Fáze | Funkce | Poznámka |
|------|--------|----------|
| A | **Multiline analýza** (např. 3 hlavní varianty) — stejný FEN, vyšší `depth`, parsování více linek z API (záleží na dostupném endpointu). | Vyžaduje rozšíření `StockfishAPIClient` / backend smlouvy. |
| B | **Eval ukazatel** (+/−) a jednoduchý **sloupcový graf** v čase (ukládat eval z historie snímků nebo tahů). | Ukládání do `StatsRecorder` nebo nové DB. |
| C | **Databáze zahájení** (ECO / název) z FEN — buď vestavěný malý dataset, nebo API. | Offline balíček Lichess opening explorer je alternativa s licencí. |
| D | **Accuracy %** po partii — porovnání tahů s engine line (jako chess.com) — náročné na výpočet a úložiště. | Po stabilním importu PGN / historie z desky. |
| E | **Analýza volné pozice**: zadání FEN / úprava desky prstem, nejen náhled ze živé partie. | Editor pozice + validace FEN. |

---

## 5. Puzzle — knihovna, vlastní sestavy, import

| Fáze | Funkce | Poznámka |
|------|--------|----------|
| **Současnost** | Denní puzzle z Lichess API, náhled desky, „Přehrát v Hře“ (statický FEN). | |
| A | **Lokální knihovna** puzzle (`SwiftData` / soubory): název, FEN, řešení (UCI), zdroj, obtížnost, štítky. | |
| B | **Import**: schránka (FEN), soubor **PGN** (partie s anotacemi), export z Lichess (CSV/JSON podle licence). | Parsovací vrstva + validace. |
| C | **Editor vlastního puzzle**: nastavit FEN, označit správné tahy (strom), hinty. | UI náročné; lze začít „jedna správná continuance“. |
| D | **Tréninkové režimy**: náhodné z kolekce, **spaced repetition** (opakování chyb), časovač na řešení. | |
| E | **Integrace s Hrou**: po vyřešení odeslat gratulaci na Watch / Live Activity (kosmetika). | |

---

## 6. Statistiky — od počítadel k insightům

| Fáze | Funkce | Poznámka |
|------|--------|----------|
| **Současnost** | Počty tahů, peak, počty snapshotů (`StatsRecorder`). | |
| A | **Relace her** (session): začátek/konec podle připojení nebo „nová hra“ z API. | Synchronizace s ESP stavem. |
| B | **Grafy v čase**: odehrané partie / puzzle týdně. | |
| C | **Odhad výkonnosti** (např. puzzle rating z vlastní knihovny) — **ne Elo FIDE**, interní metrika. | |
| D | **Export** CSV / JSON pro uživatele (GDPR friendly, lokální). | |
| E | **Porovnání s engine** u uložených partií (průměrná přesnost tahu) — navazuje na analýzu. | |

---

## 7. Další „vychytávky“ (product delight)

- **Widgety na plochu** (iOS): poslední stav partie nebo odkaz na Hru (bez full engine na pozadí).
- **Siri Shortcuts**: „Otevři CZECHMATE hru“, „Spusť hledání šachovnice“.
- **Haptika** při tahu soupeře (už částečně na Watch) — i na iPhonu volitelně.
- **iCloud záloha** vlastních puzzle a nastavení (opt-in).
- **Dvojjazyčné řetězce** (CS / EN) — příprava `Localizable.strings`.
- **TestFlight** kanál a **Telemetrie** (jen opt-in, crash reporting).

---

## 8. Závislosti a rizika

| Oblast | Riziko |
|--------|--------|
| Stockfish / chess-api | Limity rate, náklady, dostupnost více variant. |
| Velikost dat | Knihovna puzzle + PGN — úložiště a migrace verzí schématu. |
| Watch | Přenos závislý na iPhonu; throttling může zpomalit „plynulý“ odpočet — řešeno částečně lokálním odpočtem po `clockSyncAt`. |
| Právní | Lichess API — dodržovat podmínky použití; vlastní obsah v knihovně. |

---

## 9. Doporučené priority (MVP → v2 → v3)

1. **MVP+ (krátkodobě)**  
   - Doladit UI konzistenci (Stats, Onboarding pod `Theme`).  
   - Stabilní Watch + Live Activity (aktuální změny otestovat na zařízení).  

2. **v2**  
   - Lokální puzzle knihovna + import FEN ze schránky.  
   - Statistiky: relace a jednoduchý týdenní přehled.  
   - Analýza: druhá nejlepší varianta + eval číslo u pozice.  

3. **v3**  
   - Editor vlastního puzzle, PGN import, accuracy / pokročilá analýza.  
   - iPad layout, témata, případně komplikace na Watch.  

---

## 10. Související dokumenty

- `docs/planning/CZECHMATE_MASTERPLAN.md` — původní master plán projektu.  
- Kontextové podklady můžeš dávat do `context/` (viz konvence v repu).  

---

*Poslední aktualizace: 2026-04-06 — zahrnuje implementaci materiálového přehledu a časů v Live Activity / Watch; zbytek tabulky je plán k dalším sprintům.*
