# CZECHMATE — master plán rozšíření aplikace

Dokument slouží jako **strategický a implementační rámec**: co doplnit do iOS aplikace (a kde je potřeba firmware / backend), inspirováno běžnými šachovými aplikacemi (Chess.com, Lichess, DGT rozšíření), ale s **vlastní identitou** — fyzická deska, LED, MQTT/Home Assistant, offline-first k desce.

**Verze dokumentu:** 1.0  
**Stav codebase (orientační):** Stockfish přes API, evaluace tahů (`MoveEvaluation`), nápověda na LED, režim bot v NVS + návrhy tahů, učící mód, LLM trenér, souhrn partie, MQTT/HA návod v Nastavení.

---

## 1. Cíle a principy

| Princip | Význam pro CZECHMATE |
|--------|----------------------|
| **Deska je zdroj pravdy** | UI a analýza respektují stav z ESP; telefon doplňuje, ne přepisuje pravidla. |
| **Viditelná zpětná vazba** | Hráč vidí *proč* byl tah hodnocen jistě; nápovědy jsou vysvětlitelné, ne jen „šipka“. |
| **Nastavitelná náročnost** | Bot, hloubka engine, přísnost evaluace — od hobby po trénink. |
| **Soukromí a síť** | Co jde lokálně/LAN, neposílat zbytečně ven; jasné kdy je potřeba internet (Stockfish API). |
| **Parita s webem ESP** | Nové herní režimy ideálně konfigurovatelné i z webu desky, ne jen z aplikace. |

---

## 2. Inventura: co už v aplikaci je

- **Stockfish (cloud API):** nejlepší tah, eval v pěšácích, hloubka z `hintDepth`.
- **Po tahu:** automatická evaluace (`moveEvaluationEnabled`), stupně (best/good/…), historie evaluace, odměny nápověd podle kvality tahu.
- **Nápověda:** tlačítko, limit na stranu, LED zvýraznění, `lastHintSummary`.
- **Bot (firmwarová konfigurace):** `botSettings` v NVS — režim PvP vs bot, síla, strana; aplikace umí **navrhovat tah bota na LED** (`maybeSuggestBotMove`).
- **Učící mód:** Stockfish + záchranná brzda při hrubce, coach onboarding.
- **LLM trenér:** textová nápověda k pozici (závislé na modelu / síti).
- **Souhrn partie:** grafy, materiál, copy pro konec hry.
- **Připojení:** BLE / Wi‑Fi, MQTT konfigurace pro HA, styly šachovnice, párování.

**Mezery oproti „plné“ šachové aplikaci:** zpětná analýza partie v UI jako samostatný zážitek, škálování obtížnosti bota **v hlavní hře** (ne jen v dev/NVS), otevřené úlohy (puzzle knihovna v app), social/online hra (záměrně mimo scope, pokud není cíl).

---

## 3. Inspirace z webu / konkurence (mapování funkcí)

| Funkce (obvykle u konkurence) | CZECHMATE — stav | Směr implementace |
|------------------------------|------------------|-------------------|
| Analýza partie po hře | Částečně (souhrn, eval historie) | Plný „rewind“ + šipky best line |
| Puzzle / taktiky | Lichess import / náhled | Lokální sada + denní puzzle |
| Hra proti počítači | Bot přes desku + LED | Volitelně **lokální** tah bota v app + sync na desku |
| Obtížnost bota | NVS síla 6–16 | Mapovat na Elo odhad + předvolby v Hře |
| Nápovědy stupňované | Jedna úroveň nápovědy | 2–3 kroky: únik / slabina / plný tah |
| VOpening knihovna | — | Základní zkratky + odkaz na Lichess studie |
| Časové kontroly | Z desky | UI profilů uložených v app (push na desku) |
| Živý komentář | LLM částečně | Šablony + „proč tento tah“ z eval diff |
| Školení vzorců (endgame) | — | Minikurzy: K+P vs K, mat v rohu… |

---

## 4. Kreativní / diferenciátory (nad rámec kopírování)

1. **„Druhá deska“ v aplikaci** při analýze: ghost pozice vedle živé — porovnání „co kdyby nejlepší tah“.
2. **Hlasová / krátká audio nápověda** (TTS): „Braní na e4 oslabuje krále“ — generované z eval + šablony (nízká závislost na LLM).
3. **Home Assistant scénáře:** po MQTT události (mat, remíza, tah hráče) spustit automatizaci — dokumentovat topic mapu v app + příklady YAML (rozšíření současné HA sekce).
4. **Rodinný režim:** dva telefony sledují jednu desku s různými úrovněmi nápovědy (Watch už částečně companion).
5. **„Fair play“ metrika:** čas na tah vs průměr; bez odesílání mimo zařízení (lokální statistiky).
6. **LED choreografie:** nejen from–to, ale sekvence „ukáž cestu koně“ (nutná podpora firmware).

---

## 5. Implementační pilíře (workstreamy)

### 5.1 Zpětná kontrola tahů a analýza

| ID | Úkol | Popis | Závislosti |
|----|------|-------|------------|
| A1 | **Replay partie** | Krok vpřed/vzad podle `history`; synchronizace s grafem evaluace ze `EvalHistoryRecorder`. | Data ve snapshotech / historie |
| A2 | **Označení tahu v replay** | Barevně: best / OK / inaccuracy / mistake / blunder (sjednotit s `MoveEvaluationResult.grade`). | Stockfish API nebo lokální engine |
| A3 | **Alternativní linie** | Po výběru tahu v replay ukázat 2–3 hlavní varianty (multi-PV) — API rozšíření nebo druhý dotaz. | Backend API kontrakt |
| A4 | **Export PGN** | Uložit partii z historie + hlavička (datum, odkud deska). | Serializace moves |
| A5 | **Sdílení pozice** | Obrázek desky + FEN do sdílení (UIActivity). | Existující FEN builder |

### 5.2 Nápovědy (vícevrstvé a pedagogické)

| ID | Úkol | Popis |
|----|------|-------|
| H1 | **Nápověda úroveň 1** | Jen ukázat *které* pole je slabé (bez tahu) — eval pole nebo útok na krále. |
| H2 | **Úroveň 2** | Oblast 3×3 nebo šipka typu útoku. |
| H3 | **Úroveň 3** | Současné chování (plný tah + LED). |
| H4 | **Vysvětlovací text** | Šablony podle typu hrubky („ztratil jsi pole d5“) + volitelně LLM. |
| H5 | **Tréninkový limit** | V učícím módu dynamicky snížit hloubku engine pro „realističtější“ chyby soupeře. |

### 5.3 Hra proti botovi (obtížnost a UX)

| ID | Úkol | Popis |
|----|------|-------|
| B1 | **Panel „Hrát proti botovi“ v Hře** | Zrcadlit NVS `botSettings`: zapnout režim, strana, síla — bez nutnosti Developer → Deska. |
| B2 | **Mapování síly → popisek** | „Přibližně 1200 Elo“ (heuristika z depth/strength), tooltip co znamená vyšší číslo. |
| B3 | **Obtížnost pro děti** | Předvolba: hloubka 6, vypnutá brutální trestání hrubek, více nápověd. |
| B4 | **Bot tah bez LED** | Volba: bot hraje „v hlavě“ — tah zobrazit jen v app, hráč ho přehraje na desce (pro slabší hráče). |
| B5 | **Lidské zpoždění bota** | Náhodné 0.5–2 s před návrhem — psychologicky přirozenější. |

*Poznámka:* plný **automatický** tah figurkami na fyzické desce bez člověka vyžaduje hardware (robot/magnetické systémy) — mimo běžný scope; proto „human-in-the-loop“ bot.

### 5.4 Puzzle a trénink

| ID | Úkol | Popis |
|----|------|-------|
| P1 | **Vestavěná sada** | 50–200 pozic (mate ve 2, vidličky) — balíček v bundle. |
| P2 | **Synchronizace s Lichess** | Rozšířit stávající import FEN o „daily puzzle“ API (pokud licence/TOS dovolí). |
| P3 | **Režim na desce** | Puzzle vybrané v app → zvýraznit cíl na LED (vyžaduje firmware příkaz nebo hint). |
| P4 | **Streak a XP** | Jednoduché lokální statistiky (SwiftData už pro relace). |

### 5.5 Čas, partie a profily

| ID | Úkol | Popis |
|----|------|-------|
| T1 | **Profily času** | 3+5, 15+10, rapid — uložit v app, jedním tahem nastavit na desku (pokud API). |
| T2 | **Oddělené „herní profily“** | Jméno hráče, barva preferencí, default bot síla. |

### 5.6 Watch, widgety, Live Activity

| ID | Úkol | Popis |
|----|------|-------|
| W1 | **Live Activity: eval tečky** | Zelená/žlutá/červená podle posledního `lastMoveEvaluation`. |
| W2 | **Watch: nápověda stupeň** | Haptic při blunderu (už částečně přes iPhone). |

### 5.7 Firmware a web (koordinace)

| ID | Úkol | Popis |
|----|------|-------|
| F1 | **MQTT: bohatší telemetrie** | Topic pro „move_played“, „game_end“ s PGN fragmentem — pro HA automatizace. |
| F2 | **REST pro puzzle cíl** | Endpoint nebo rozšířený hint pro „cílové pole“ v puzzle režimu. |

---

## 6. Fázování (doporučené pořadí)

### Fáze 0 — Hotfixy a konzistence (1–2 týdny)
- Sjednotit texty a dostupnost bota / evaluace mezi `GameContainerView` a legacy `GameView`.
- Zobrazit `lastMoveEvaluation` konzistentně ve všech layoutech (standard / wide / board-only).

### Fáze 1 — Viditelná kvalita tahů (2–4 týdny)
- A1 + A2: replay s barevným hodnocením tahů.
- H4: krátké texty k hrubkám (šablony).
- B1: uživatelský panel bota v hlavní hře (zápis do NVS přes stávající API).

### Fáze 2 — Hloubka tréninku (4–8 týdnů)
- H1–H3: stupňovaná nápověda.
- A3: alternativní varianty (omezeně, 2 PV).
- P1: vestavěné puzzle + základní UI.

### Fáze 3 — Ekologie a „wow“ (8+ týdnů)
- MQTT rozšíření + dokumentace HA (F1).
- LED choreografie (F2 + firmware).
- Volitelně lokální engine (Swift Stockfish) pro offline eval — velký balík, samostatné ROzhodnutí.

---

## 7. Technické rizika a předpoklady

- **Stockfish API:** latence, limity, náklady — pro replay s 40+ tahy zvážit batch nebo lokální engine.
- **BLE vs Wi‑Fi:** těžké operace (multi-PV) jen na Wi‑Fi nebo na pozadí s indikátorem.
- **Baterie:** kontinuální analýza v replay — throttling.
- **Konzistence FEN:** vždy ověřit `board + side + castling + en passant` při analýze historie.

---

## 8. Měření úspěchu (produkt)

- Délka relace v učícím módu vs běžném.
- Počet použitých nápověd na partii / trend dolů při opakování.
- Podíl uživatelů, kteří zapnou bot režim z nového panelu (B1).
- Crash-free sessions, čas do první úspěšné MQTT konfigurace (HA).

---

## 9. Shrnutí priorit (executive)

1. **Zpětná analýza partie v UI** (replay + barvy + krátký komentář) — největší dopad na „správnost tahů“.  
2. **Bot a obtížnost dostupné normálnímu uživateli** — už je v NVS, chybí produktový obal.  
3. **Stupňované nápovědy** — pedagogický rozdíl oproti konkurenci s čistě digitální deskou.  
4. **Puzzle balíček** — návyk a denní návrat.  
5. **HA/MQTT hlubší integrace** — loajalita power-userů.  

---

*Tento dokument lze dělit na samostatné issue/EPIC v trackeru; každý řádek tabulky může být jedno issue s odkazem na ID (A1, H2, …).*
