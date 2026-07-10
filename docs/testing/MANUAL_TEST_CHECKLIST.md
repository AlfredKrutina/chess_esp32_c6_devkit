# Opening Trainer — manuální HW checklist (v1.0 release gate)

**Účel:** Ověřit §21 release gate v [OPENING_TRAINING_PLAN.md](../reference/OPENING_TRAINING_PLAN.md) na reálné desce.  
**HW:** ESP32-C6 CzechMate, fyzická šachovnice, LED, Flutter klient (BLE nebo Wi‑Fi).  
**Čas:** ~45–60 min pro plný běh; minimální gate = sekce **A + B + C**.

---

## Před testem

| # | Kroky | Očekávání |
|---|--------|-----------|
| 0.1 | Flash firmware z `main` (`idf.py flash`) | Boot bez WDT resetu |
| 0.2 | Deska v **standardní startovní pozici** | Matrix = 32 figurek |
| 0.3 | Flutter připojen (BLE nebo HTTP) | Snapshot `/api/status` nebo GATT OK |
| 0.4 | (Volitelně) HTTP smoke: `./scripts/test_opening_api.sh http://<board-ip>` | `action: start` vrátí 200, status obsahuje `opening_training` |

**Poznámka:** Při virtuálním soupeři očekávej **checkpoint** — fyzická deska se musí srovnat s logickou.

---

## A — Gate G2: 3 linie × Learn + Drill

Každá linie: **Fyzický soupeř** (default), nejdřív **Učení**, pak **Drill** (po dokončení Learn).

### A1 — Italská hra bílých (`italian_giuoco_white`)

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | Katalog → Italská hra → Fyzicky + **Učení** | Lekce startuje; rationale jen na 1. tahu |
| 2 | Tah `e2→e4` na desce | LED + text „e2 → e4“; komentář k tahu |
| 3 | Po `e7e5` LED ukáže tah soupeře | Přesuneš černého pěšáka |
| 4 | Dokonči všechny 4 hráčovy tahy | `complete` + endgame LED; ★1 uložena |
| 5 | Znovu otevři linii → **Drill** | Bez komentářů k tahům; počítadlo chyb |
| 6 | Dokonči Drill s ≤2 chybami | ★2 |

### A2 — Sicilská ODB černých (`sicilian_odb_black`)

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | Katalog → Sicilská → Fyzicky + **Učení** | Bílý `e4` se provede automaticky před tvým tahem |
| 2 | První hráčův tah černých | Validace UCI na desce |
| 3 | Dokonči Learn | ★1 |
| 4 | Drill s ≤2 chybami | ★2 |

### A3 — Španělská Berlín bílých (`spanish_berlin_white`)

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | Learn screen L12 nebo katalog → **Učení** | Mode picker (ne skrytý default) |
| 2 | Projdi linii s fyzickým soupeřem | Komentáře + idea viditelné v Learn |
| 3 | Dokonči Learn + Drill | ★1 → ★2 |

**Gate G2 ✅** pokud všechny tři linie projdou Learn i Drill bez zaseknutí.

---

## B — Gate G3/G4: Režimy, checkpoint, cancel

### B1 — Virtuální soupeř + checkpoint

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | `italian_giuoco_white`, **Virtuálně** + Učení | Soupeř táhne bez tvého přesunu figurek |
| 2 | Po ~4. ply (checkpoint) | UI „Srovnej desku“; miniboard zvýrazní rozdíly |
| 3 | Nech desku nesrovnanou → **Pokračovat** | Tlačítko disabled / 409 přes API |
| 4 | Srovnej desku → **Deska srovnaná — pokračovat** | Lekce pokračuje |

### B2 — Na čas (Timed)

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | Linie s ★≥2 → **Na čas** 90 s | Odpočet v app bar |
| 2 | Dokonči v limitu | ★3 |
| 3 | (Volitelně) Nech vypršet čas | Lekce se ukončí, bez ★3 |

### B3 — Cancel + matrix guard

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | Spusť libovolnou lekci | Opening aktivní |
| 2 | Během lekce zvedni figuru mimo tah (ghost) při **virtuálním** soupeři | Matrix guard **nesmí** zamrznout hru |
| 3 | Zavři lekci (×) | Návrat do normální hry |
| 4 | Zahraj normální tah / puzzle | Matrix guard znovu funguje |

**Gate G3/G4 ✅** pokud checkpoint, timed a cancel projdou.

---

## C — Gate P6: 2 mirror páry (e4 + d4)

Vyžaduje ★★ na hlavní linii před odemčením Mirror.

### C1 — e4 pár: Italská bílých ↔ Petrova černých

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | `italian_giuoco_white` má ★≥2 | Mirror odemčen |
| 2 | Zvol **Mirror — protistrana** | Načte se `petrov_black` |
| 3 | Dokonči s ≤2 chybami | ★4 na `petrov_black` |

### C2 — d4 pár: Londýnský systém ↔ Slav černých

| Krok | Akce | Očekávání |
|------|------|-----------|
| 1 | `london_system_white` ★≥2 | Mirror odemčen |
| 2 | Mirror → `slav_defence_black` | Protistrana d4 repertoáru |
| 3 | Dokonči | ★4 |

**Gate P6 ✅** pokud oba páry fungují a vedou na správnou protistranu.

---

## D — Pedagogika a UX (P1–P5)

| # | Test | Očekávání |
|---|------|-----------|
| D1 | Špatný tah v Learn (např. `Bb5` místo `Bc4` u italštiny) | Text „Ne Bb5…“ (`common_mistakes`), ne `Stav: wrong` |
| D2 | 3× špatný tah na stejném ply | `mistake_hint` + LED ukáže správný tah |
| D3 | EN locale v aplikaci | Anglické `idea`, `steps`, feedback |
| D4 | Miniboard během lekce | Pozice + hint from→to; checkpoint fialové pole |
| D5 | Progress po restartu app | ★ zůstanou (SharedPreferences) |

---

## E — BLE-only build (volitelný HW profil)

Pro desku s firmware `CONFIG_CHESS_ENABLE_WEB_SERVER=n`:

| # | Kroky | Očekávání |
|---|--------|-----------|
| E1 | Build: viz CI job `ble-only` v `firmware-build.yml` | Link bez chyb |
| E2 | Flash BLE-only firmware | UART `WEB` hlásí HTTP disabled |
| E3 | Flutter přes BLE: start Learn `italian_giuoco_white` | Stejná lekce jako HTTP build |
| E4 | Hint / cancel / complete přes GATT | Parita s HTTP |

---

## F — Web parita (G5, pokud HTTP build)

| # | Kroky | Očekávání |
|---|--------|-----------|
| F1 | Browser → board IP → Opening trainer | Katalog 41 linií |
| F2 | Learn + Drill jedné linie | Stejné feedback texty jako Flutter |
| F3 | `localStorage` `opening_progress_v1` | Hvězdičky po dokončení |

---

## G — Gameplay policy profily (menuconfig, PR #20)

Ověření po změně Kconfig. Vyžaduje flash s příslušným `SDKCONFIG_DEFAULTS`.

### G1 — FULL (produkce, default)

| # | Kroky | Očekávání |
|---|--------|-----------|
| G1.1 | Boot monitor | `GAMEPLAY_POLICY: profile=FULL MG=1 ER=1 MH=1` |
| G1.2 | Zvedni 2 figurky najednou | Matrix guard active, žlutá/modrá LED |
| G1.3 | Nelegální tah na desce | Červené pole + lock (`error_state.active`) |
| G1.4 | Po validním tahu | Modré/žluté hinty podle LED guidance |

### G2 — LITE (`sdkconfig.defaults.gameplay_lite`)

| # | Kroky | Očekávání |
|---|--------|-----------|
| G2.1 | Boot monitor | `profile=LITE`, `MG=0` |
| G2.2 | Zvedni 2 figurky | **Žádný** guard, hra pokračuje (UART warning) |
| G2.3 | Nelegální tah | UART chyba, **bez** červené LED, **bez** locku |
| G2.4 | `/api/status` | `matrix_guard_active=false`, `error_state.active=false`, `gameplay_profile":"LITE"` |

### G3 — DEV (`sdkconfig.defaults.gameplay_dev`)

| # | Kroky | Očekávání |
|---|--------|-----------|
| G3.1 | Opening Learn fyzický soupeř | Guard se **ne**aktivuje při běžných tazích |
| G3.2 | Nelegální tah | ER lock + LED jako FULL |
| G3.3 | Ghost figurka mimo tah | Bez guard pause (jako LITE u MG) |

**Gate gameplay ✅** pokud G1–G3 odpovídají tabulce v [MENUCONFIG_FEATURES_PLAN.md](../reference/MENUCONFIG_FEATURES_PLAN.md) §10.

---

## H — Hall V2 + STM32 auto-flash (volitelné, HW V2)

Build: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build flash`  
Zapojení: [ZAPOJENI_ESP_STM4.md](../reference/ZAPOJENI_ESP_STM4.md)

| # | Kroky | Očekávání |
|---|--------|-----------|
| H1 | První boot virgin STM (nebo po `chip erase` ESP) | `STM32_AUTO` → flash z oddílu `stm32_fw` |
| H2 | Po flashi | `STM32_I2C_BL: [hall_probe] seg0 addr 0x30 OK` |
| H3 | Druhý boot (stejný STM) | `auto-flash přeskočen` + hall_probe OK |
| H4 | Výměna STM / prázdný čip, NVS beze změny | `NVS … ale Hall neodpovídá — vynucuji auto-flash` |
| H5 | Matrix scan | `HALL_I2C` bez WARN na seg0; pole reagují na magnet |
| H6 | UART | `CLI HALL PROBE 0` → `Hall seg0 probe OK` |

---

## Shrnutí gate (zaškrtni po testu)

| ID | Kritérium | HW | Poznámka / datum |
|----|-----------|-----|----------------|
| G2 | 3× Learn + Drill | ☐ | |
| G3 | Matrix guard bez regrese | ☐ | |
| G4 | 4 režimy na FW | ☐ | |
| G5 | Flutter ≈ web | ☐ | |
| G6 | Curriculum unlock | ☐ | auto test CI |
| G7 | Progress po restartu | ☐ | |
| P1 | Žádný `Stav:` v opening UI | ☐ | |
| P2 | Rationale jen ply 0 | ☐ | |
| P3 | EN locale steps | ☐ | |
| P4 | common_mistakes ≥10 linií | ☐ | auto test CI |
| P5 | Miniboard | ☐ | |
| P6 | 2 mirror páry e4+d4 | ☐ | |
| P7 | L10/L12 mode picker | ☐ | |
| GP | Gameplay FULL / LITE / DEV (§G) | ☐ | menuconfig PR #20 |

**v1.0 release:** všechny řádky G* a P* ✅ (G6/G7/P4 částečně pokryto CI — viz `opening_release_gate_test.dart`).

---

## Automatizované doplňky (CI)

- `openings-catalog.yml` — 41 linií, UCI, mirror-symmetric, sync
- `flutter-test.yml` — catalog, progress, curriculum, UX, release gate
- `firmware-build.yml` — full HTTP + BLE-only + gameplay-lite + gameplay-dev + hall-v2 profily
- `scripts/test_opening_api.sh` — HTTP smoke na desce
