# Komplexní plán: UI živé aktivity (Live Activity) — iPhone a Apple Watch

**Verze dokumentu:** 1.0  
**Cíl:** Profesionální, konzistentní a informačně hutné zobrazení partie v **Live Activity** na iOS (Lock Screen, Dynamic Island) a spolehlivé chování v ekosystému **Apple Watch** (Smart Stack / zrcadlení aktivity z iPhonu + vlastní companion UI na hodinkách).  
**Stav v kódu (východisko):** `ChessMatchLiveActivity.swift`, `ChessMatchActivityAttributes.swift`, `MatchLiveActivityManager.swift`, `BoardCompanionSync.swift`.

---

## 1. Shrnutí pro vedení produktu

| Oblast | Dnešní stav | Směr zlepšení |
|--------|-------------|----------------|
| **Lock Screen** | Funkční VStack: název, tahy, materiál, subtitle, šach, časovač | Jasná hierarchie, stavy partie (šach/mat/remíza), lepší čitelnost při slabém osvětlení, přístupnost |
| **Dynamic Island** | Rozšířené regiony + compact/minimal | Sjednotit význam compact trailing (dnes často jen bílý čas), přidat kontext tahu, šetřit místo |
| **Watch** | Live Activity na hodinkách typicky **odvozená od iPhonu**; data též **WatchConnectivity** | Oddělit: (A) vzhled LA z iPhonu, (B) vlastní complication / glance na Watch app — plán pokrývá obojí |
| **Data / frekvence** | Throttle v `BoardCompanionSync` (0,85–1,2 s) | `staleDate`, relevance, případně šetrnější aktualizace mimo aktivní časovač |

**Měřitelné výsledky:** vyšší čitelnost na první pohled (test „3 sekundy“), méně uživatelských dotazů „co to znamená“, soulad s Apple Human Interface Guidelines pro Live Activities a WidgetKit.

---

## 2. Inventura současné implementace

### 2.1 Sdílený model

- **`CZECHMATEShared/ChessMatchActivityAttributes.swift`**  
  - `ChessMatchAttributes` + `ContentState`: `moveCount`, `currentPlayer`, časy, `subtitle`, `inCheck`, `gameEnded`, `advantageSummary`, `timerActive`.  
  - **Mezera:** žádné explicitní pole pro **typ konce partie**, **poslední tah SAN**, **stav připojení**, **FEN hash** (pro debug), **verze schématu** (pro migrace).

### 2.2 Aplikace (iOS)

- **`MatchLiveActivityManager.swift`**  
  - Start/update/end podle snapshotu; konec při `connectionStopped` nebo konci hry.  
  - `makeContentState` skládá `subtitle` z posledního tahu + `MaterialEvaluator.advantageSummary`.  
  - **Mezera:** uživatelský přepínač v Nastavení je zmíněn jen v logu („disabled in Settings“) — **chybí reálný UI toggle** v `SettingsTabView` (sekce Hodinky / Live Activity je jen text).

### 2.3 Widget extension

- **`CZECHMATEWidgets/ChessMatchLiveActivity.swift`**  
  - Lock Screen: jeden vertikální blok, zelený `activityBackgroundTint`.  
  - Dynamic Island: leading/trailing/bottom + compact/minimal.  
  - **Mezera:** viz §4–5 (typografie, stavy, kompaktní režimy, lokalizace řetězců přímo ve view).

### 2.4 Propagace na hodinky

- **`BoardCompanionSync`** — throttling LA + push na Watch přes `WatchConnectivityBridgeEnhanced`.  
- Watch app **nezobrazuje** stejný SwiftUI kód jako widget; zlepšení „na hodinkách“ = **(1)** kvalita dat v LA ze iPhonu + **(2)** úpravy **Watch UI** (glance, complication), nikoli duplikát `ActivityConfiguration` na watchOS (ten je u většiny scénářů řízený iPhonem).

---

## 3. Design principy (profi rámec)

### 3.1 Informační hierarchie (priorita zobrazení)

1. **Kdo je na tahu** + **šach** (okamžitá akce pro hráče).  
2. **Čas** (pokud `timerActive`) — aktivní strana vizuálně dominantní.  
3. **Poslední tah** nebo **krátký stav** („e4“, „O-O“, „braní na e5“).  
4. **Materiál / eval jednou větou** (`advantageSummary`) — sekundární.  
5. **Meta** (počet tahů, CZECHMATE) — terciární.

### 3.2 Konzistence značky

- Barvy: sladit s `Theme.accent` / existující šachovnicí — **ne** hardcodovat jediný zelený tint; zvážit **Asset catalog** „LiveActivityBackground“ (light/dark).  
- Ikony: SF Symbols kde dává smysl (`crown`, `checkerboard.rectangle`, `timer`, `flag.checkered`).

### 3.3 Přístupnost

- **Dynamic Type:** otestovat `.extraLarge` — zkrátit texty nebo `minimumScaleFactor`.  
- **VoiceOver:** smysluplné **accessibility labels** pro compact region (koruna + číslo bez kontextu je slabé).  
- **Kontrast:** ověřit WCAG na Lock Screen přes různé tapety (světlé/tmavé).

### 3.4 Lokalizace

- Veškeré uživatelské řetězce z **Localizable.strings** (ne natvrdo v `ChessMatchLiveActivity`).  
- `playerLabel` / formáty času sdílet s hlavní aplikací nebo malým „WidgetCopy“ enum.

---

## 4. Rozšíření datového modelu (`ContentState`)

**Fáze A (doporučeno jako první):** rozšířit `ContentState` o pole s výchozími hodnotami pro zpětnou kompatibilitu:

| Pole | Typ | Účel |
|------|-----|------|
| `schemaVersion` | `Int` | Migrace obsahu při update aplikace |
| `phase` | enum (`inProgress`, `checkmate`, `stalemate`, `draw`, `paused`, `disconnected`) | Jednotné UI pro konec / pauzu / výpadek |
| `lastMoveSAN` | `String?` | Krátký, čitelný tah (pokud lze spočítat z historie) |
| `connectionHint` | `String?` | Volitelně „Čeká na desku…“ při degradaci (opatrně — ne spamovat) |
| `whiteLabel` / `blackLabel` | `String?` | Volitelné jména hráčů z budoucího API |

**Fáze B (volitelné):**  
- `fenCompact` (zkrácený hash + poslední sync) — jen pro interní debug build.  
- Push token workflow (APNs) — viz §10.

**Implementační poznámka:** po změně `ContentState` synchronně upravit `makeContentState` v `MatchLiveActivityManager` a všechny větve UI ve widgetu.

---

## 5. iPhone — Lock Screen

### 5.1 Layout varianty

- **Standardní výška:** současný VStack je OK; přidat **horizontální uspořádání** pro časovač (dva sloupce) už máte — doladit **vyvážení** (stejná šířka sloupců, baseline).  
- **Stav ukončení partie:** před ukončením aktivity zobrazit **1–2 s „final card“** (`ActivityContent` s `phase == .checkmate` + text) pak `end` s `dismissalPolicy` — vyžaduje orchestraci v manageru (krátký delay vs okamžité `end`).

### 5.2 Vizuální styl

- `activityBackgroundTint` → **sekundární materiál** nebo vlastní barva z assetů; přidat **`.activitySystemActionForegroundColor`** kde Apple doporučuje pro tlačítka (pokud přibudou).  
- Oddělovače mezi sekcemi (tenký `Divider`) pro skenování očima.

### 5.3 `widgetURL` a deep link

- Dnes: `czechmate://game`.  
- Rozšířit o **query** `?source=liveactivity` pro analytiku / budoucí A/B (bez rozbití existujícího handleru v `ContentView`).

---

## 6. iPhone — Dynamic Island

### 6.1 Problém compact trailing

- Nyní: při běžícím časovači se zobrazuje **jen bílý čas** — pro uživatele hrajícího černé je **zavádějící**.  
- **Návrh:**  
  - **Varianta A:** vždy zobrazit **aktivní** čas (strana na tahu) — jedna hodnota.  
  - **Varianta B:** monospaced „`W·B`“ ve formátu `3:12·2:59` (krátké, nutno ověřit šířku na mini zařízeních).  
  - **Varianta C:** ikona strany + čas aktivního hráče.

### 6.2 Expanded regions

- **Leading:** figurka + `advantageSummary` — OK; zvážit **stack** s `lastMoveSAN`.  
- **Trailing:** hráč na tahu + oba časy — zvážit zvýraznění aktivního.  
- **Bottom:** `subtitle` — max 2 řádky; při přetečení **trim** s `…` a plný text na Lock Screen.

### 6.3 Minimal

- Figurka je dobrý diferenciátor; při **šachu** přidat malý badge (tečka/ikonka) pokud to nepřekrývá glyph.

---

## 7. Apple Watch — strategie (dvě kolejnice)

### 7.1 Kolej 1: Live Activity ve Smart Stacku (řízeno iPhonem)

- Uživatel **nepíše** widget pro watchOS stejným souborem — systém zobrazuje aktivitu z telefonu.  
- **Akce:** maximalizovat kvalitu **ContentState** a **Lock Screen layoutu** (čitelnost na malém náhledu); testovat na **Series 9/10/Ultra** v Simulatoru + reálném zařízení.  
- **Nastavení:** jasná sekce v aplikaci — „Zobrazovat živou aktivitu“ + odkaz na **Systémové nastavení → CZECHMATE → Live Activities**.

### 7.2 Kolej 2: Nativní Watch app (`CZECHMATE_W`)

- **Complication** (budoucí): poslední tah / „na tahu bílý“ / ikona aplikace.  
- **Glance / hlavní obrazovka:** srovnat typografii a barvy s LA na iPhonu (stejná slovní zástupná značka pro „na tahu“).  
- **Synchronizace:** dokumentovat latenci `applicationContext` vs `sendMessage` (už částečně v `WatchUnifiedSessionModel`).

---

## 8. `MatchLiveActivityManager` — chování a životní cyklus

### 8.1 Kdy aktivitu zobrazovat

- Dnešní pravidlo: `!gameEnded && (isTimerRunning || moveCount >= 1)`.  
- **Revize:** zvážit zobrazení už při **moveCount == 0** pokud běží časovač nebo je **explicitní „hra zahájena“** ve statusu (aby LA nezmizela na začátku).  
- **Odpojení:** dnes `connectionStopped()` ukončí LA — alternativa: **jedna** aktualizace stavu `phase = .disconnected` a teprve po timeoutu `end` (uživatelské nastavení).

### 8.2 `staleDate` a relevance

- Nastavit `staleDate` např. na **očekávaný další poll** + buffer, aby systém věděl, že data mohou být zastaralá.  
- Zvážit `relevanceScore` / **ActivityKit** API podle cílového iOS (dokumentace pro daný rok SDK).

### 8.3 Nastavení v aplikaci

- Přidat `@AppStorage` např. `czechmate.liveActivityEnabled` a v `applySnapshot` **časný return** před `Activity.request`.  
- Synchronizovat s copy v **Nastavení → Hodinky a Live Activity** (dnes jen statický text).

---

## 9. Throttling a výkon

| Parametr | Současně | Návrh |
|----------|----------|--------|
| Interval bez časovače | 1,2 s | Ponechat nebo 1,5 s při stejné `moveCount` |
| S časovačem | 0,85 s | Ponechat; při nezměněném stavu jen **update `staleDate`** bez plného `update` kde to API dovolí |
| Po ukončení hry | okamžitý `end` | Volitelný **záverečný update** + `end(after:)` |

**Battery / CPU:** Live Activity update není zdarma — měřit Energy Log v Instruments při 30 min hře s časovačem.

---

## 10. Pokročilá fáze (volitelná)

1. **Push notifications pro Live Activity** (APNs) — update z backendu bez otevřené aplikace (náročné na infrastrukturu).  
2. **Interaktivní tlačítka** (když je systém a entitlements dovolí) — např. „Pauza“ pouze pokud MCU podporuje a je to bezpečné.  
3. **Více aktivit** — vyloučit; držet jednu aktivitu na „aktuální relaci“ (`matchId` už existuje).

---

## 11. Testovací matice

| Scénář | Lock Screen | DI compact | DI expanded | Watch Stack |
|--------|-------------|------------|-------------|-------------|
| Hra bez časovače | ✓ | ✓ | ✓ | ✓ |
| Časovač, tah bílého | ✓ | ✓ | ✓ | ✓ |
| Časovač, tah černého | ✓ | **regrese?** | ✓ | ✓ |
| Šach | ✓ | ✓ | ✓ | ✓ |
| Mat (před end) | ✓ | ✓ | ✓ | ✓ |
| Odpojení desky | ✓ | ✓ | — | ✓ |
| Live Activities vypnuté v systému | žádná aktivita | — | — | — |
| Dynamic Island absent (SE) | — | N/A | N/A | — |

**Přístroje:** iPhone s DI, iPhone bez DI, malý simulátor.

---

## 12. Fázování dodávky (roadmapa)

| Fáze | Rozsah | Odhad (relativní) |
|------|--------|-------------------|
| **M1** | `ContentState.phase` + oprava compact trailing + `AppStorage` toggle v Nastavení | základ |
| **M2** | Lock Screen vizuální pass (tint, typografie, lokální řetězce) + `staleDate` | střední |
| **M3** | Final state před `end` + odpojení (debounce) | střední |
| **M4** | Watch complication + sjednocení copy s iOS | větší |
| **M5** | Push / interakce | volitelné |

---

## 13. Rizika a závislosti

- **Binární velikost** widget extension — přidávání assetů a fontů sledovat.  
- **SwiftData / hlavní app** nesmí tahat těžké závislosti do extension — logika **materiálu** už je v `MaterialEvaluator`; držet ve sdíleném modulu.  
- **Review:** Apple občas zpřísní pravidla pro Live Activities; držet se **oficiální dokumentace** k cílovému `IPHONEOS_DEPLOYMENT_TARGET`.

---

## 14. Kontrolní seznam „hotovo“

- [ ] Uživatel může vypnout LA v CZECHMATE bez mystických logů.  
- [ ] Compact trailing dává smysl pro obě barvy figurer.  
- [ ] Všechny viditelné řetězce lokalizovatelné.  
- [ ] VoiceOver popisuje compact stav smysluplně.  
- [ ] Žádný zbytečný update při nezměněném snapshotu (kde to jde).  
- [ ] Dokumentace pro podporu: „jak zapnout Live Activities na iOS a co zobrazujeme na Watch“.

---

## 15. Odkazy na soubory (implementační vstup)

```
CZECHMATE/CZECHMATEWidgets/ChessMatchLiveActivity.swift
CZECHMATE/CZECHMATEShared/Sources/CZECHMATEShared/ChessMatchActivityAttributes.swift
CZECHMATE/CZECHMATE/Services/MatchLiveActivityManager.swift
CZECHMATE/CZECHMATE/Services/BoardCompanionSync.swift
CZECHMATE/CZECHMATE/Features/Settings/SettingsTabView.swift   (sekce Watch / LA)
CZECHMATE/CZECHMATE_W Watch App/                               (kolej Watch UI)
```

---

## 16. Stav implementace oproti plánu (audit)

**Datum auditu:** 2026-04-13  
**Závěr:** Z plánu **není** vše implementováno. Hotová je převážně **M2 (část)** a **oprava DI compact trailing**; **M1, M3, M4, M5** a řada bodů z §8–9 zůstávají.

### 16.1 Hotovo (v kódu)

| Oblast | Plán | Realita |
|--------|------|---------|
| Lock Screen | §5 — hierarchie, oddělovače, čitelnost | Ano — hlavička s ikonou, `Divider`, blok „Na tahu“, materiál s ikonou, pilulky časovače, `activityBackgroundTint` v barvě akcentu (`LiveActivityPalette` ≈ `Theme.accent`). |
| `widgetURL` | §5.3 — `?source=liveactivity` | Ano — `czechmate://game?source=liveactivity`; deep link v `ContentView` host `game` funguje. |
| Dynamic Island compact trailing | §6.1 — čas **hráče na tahu** | Ano — `activePlayerSeconds` / `compactTrailingView`. |
| DI expanded | §6.2 — zvýraznění aktivního času | Ano — velký čas aktivního hráče + řádek obou časů. |
| Minimal + šach | §6.3 | Ano — červená tečka při `inCheck`. |
| Přístupnost (základ) | §3.3 | Částečně — VoiceOver labely u hlavičky, compact trailing, minimal; ne systematický průchod Dynamic Type. |
| BLE / sdílený kód | mimo LA plán | `BLEStateMachine` oprava `startScanning()` z MainActor kontextu (guard `self`). |

### 16.2 Nehotovo / jen zčásti

| Oblast | Plán | Stav |
|--------|------|------|
| **Rozšíření `ContentState`** | §4 — `phase`, `schemaVersion`, `lastMoveSAN`, … | **Ne** — model je stále původní (`ChessMatchActivityAttributes.swift`). |
| **`staleDate` / relevance** | §8.2 | **Ne** — v `MatchLiveActivityManager` je stále `staleDate: nil`. |
| **Přepínač v CZECHMATE** | §8.3 — `czechmate.liveActivityEnabled` + Nastavení | **Ne** — `SettingsTabView` má jen statický text; manager kontroluje jen `ActivityAuthorizationInfo().areActivitiesEnabled` (systém). |
| **Final card před `end`** | §5.1, §9, M3 | **Ne** — stále okamžité `end` při konci hry / odpojení. |
| **Odpojení → `phase.disconnected`** | §8.1 | **Ne** — `connectionStopped()` rovnou ukončí aktivitu. |
| **Pravidlo „kdy zobrazit LA“** | §8.1 revize (`moveCount == 0` + časovač) | **Nezměněno** — stále `moveCount >= 1 \|\| isTimerRunning`. |
| **Throttling / diff** | §9 — update jen při změně / `staleDate` | **Ne** — `BoardCompanionSync` jen časový interval, vždy volá `applySnapshot`. |
| **Lokalizace** | §3.4 — Localizable.strings pro vše | **Částečně** — mix `String(localized:)` a natvrdo českých řetězců v widgetu. |
| **Asset catalog** pro pozadí | §3.2, §5.2 | **Ne** — barvy v kódu. |
| **Apple Watch** | §7 — toggle copy, complication, sjednocení UI | **Ne** — complication / úpravy Watch UI nejsou v rámci tohoto plánu dodané. |
| **M5 Push / interakce** | §10 | **Ne**. |
| **Kontrolní seznam §14** | | Většina položek **nezaškrtnutá** (toggle v app, plná lokalizace, dokumentace podpory, diff updates). |

### 16.3 Doporučené další kroky (priorita)

1. **M1 dokončit:** `@AppStorage` + Toggle v **Nastavení → Hodinky a Live Activity** + časný return v `MatchLiveActivityManager.applySnapshot`.  
2. **`staleDate`** v `ActivityContent` podle intervalu pollování / časovače.  
3. **Volitelně `phase` + krátký finální stav** před `end` (vyžaduje rozšíření `ContentState`).  
4. **Watch** až jako samostatná iterace (complication + copy).

---

*Tento dokument je určen jako živý plán: po každé dodané fázi aktualizovat tabulku stavů v §2 a zaškrtávat §14.*
