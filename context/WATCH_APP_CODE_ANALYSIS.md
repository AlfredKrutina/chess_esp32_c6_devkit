# Analýza kódu aplikace CZECHMATE pro Apple Watch

Rozsah: target **CZECHMATE_W Watch App** a jeho závislosti na **CZECHMATEShared**. Cíl: přehled architektury, toků dat, rizik a mezer oproti produkčnímu chování.

---

## 1. Struktura projektu (soubory)

| Soubor | Role |
|--------|------|
| `CZECHMATE_WApp.swift` | `@main`, `WindowGroup` → `RootWatchView`, log při init |
| `RootWatchView.swift` | Rozcestí úvod / hlavní UI podle `@AppStorage`, injektuje `watchIntroReset` |
| `WatchOnboardingView.swift` | 3stránkový průvodce + definice `EnvironmentKey` pro reset úvodu |
| `ContentView.swift` | Hlavní UI (~920 řádků): TabView (5 stránek), šachovnice, tahy, lampa, síť |
| `WatchUnifiedSessionModel.swift` | Jediný session model: BLE + WatchConnectivity, haptic, conflict resolver |
| `WatchWristAmbientDimming.swift` | Modifier pro ztmavení při AOD / neaktivní scéně |
| `WatchAppLog.swift` | `Logger` + `#if DEBUG` printy (`[Watch][STAGING/WC/BLE]`) |
| `Watch AppTests` / `UITests` | Šablony bez reálných testů |

Žádný `WKExtensionDelegate` ani complications — výhradně SwiftUI lifecycle.

---

## 2. Tok spuštění a stavů

```
App init → WindowGroup → RootWatchView
  ├─ onboardingCompleted == false → WatchOnboardingView
  │     └─ „Začít“ → main.async → onboardingCompleted = true
  └─ onboardingCompleted == true → ContentView
        .environment(\.watchIntroReset) { onboardingCompleted = false }
        .onAppear → model.activate()   [BLE + WCSession]
        .onDisappear → model.deactivate()
```

**Důsledky:**

- **BLE a WC** žijí v životním cyklu `ContentView`. Jakýkoli `onDisappear` celého `ContentView` shodí BLE (viz `WATCH_BLE_DEEP_ANALYSIS_AND_PLAN.md` §2.1 / §6 krok 2).
- Po resetu úvodu z „Více“ se `ContentView` zničí a znovu vytvoří → nová instance `WatchUnifiedSessionModel` (`@State`), čistý stav.
- **Singleton** `MoveConflictResolver.shared` přitom přežívá napříč tímto přepnutím — teoreticky zřídka zanechané tickety z předchozí session (okrajový případ).

---

## 3. WatchUnifiedSessionModel (jádro logiky)

- **`@MainActor` + `@Observable`**: veškerá herní data a režim připojení pro SwiftUI.
- **Režimy:** `.determining` → po startu BLE většinou `.bleDirect`; po 10 s bez spojení `.watchConnectivity` (BLE SM zahozen).
- **BLE:** vlastní `BLEStateMachine(deviceType: .watch)`, delegát pro stav + snapshot JSON → `UnifiedGameState`.
- **WatchConnectivity:** `ensureWatchConnectivitySession()` vždy při `activate()` (kvůli systémovým hláškám), delegát pro kontext / zprávy / reachability.
- **Příkazy na desku:** `sendCommand` (BLE) vs `sendMessage` (WC, vyžaduje `isReachable` pro spolehlivé odpovědi).
- **Tahy v BLE režimu:** `executeMove` používá `MoveConflictResolver` + **500 ms** zpoždění (priorita iPhonu); u čistého Watch↔deska bez souběžného tahu z iPhonu je to jen dodatečná latence.
- **readNetworkInfoFromBLE** po připojení: parsování JSON, zatím hlavně log (rozšíření přes iPhone bridge je v komentáři).

**Rizika / poznámky:**

- `connectionLost` v `.watchConnectivity`: kontroluje jen `activationState == .activated`, ne `isReachable` — banner v UI to doplňuje textem o otevření iPhone app.
- Fallback po 10 s může uživatele přepnout do režimu, kde bez iPhonu v popředí „nic nejde“, i když problém byl dočasný BLE.

---

## 4. ContentView (UI)

- **TabView** (stránkovaný): Partie | Deska | Tah | Lampa | Více.
- **Šachovnice:** `SharedChessBoardView` + `FENPlacementParser` ze shared balíčku — konzistentní s iPhonem.
- **Časomíry:** `TimelineView` + lokální odečet od `clockSyncAt` / `lastUpdate` — rozumný kompromis bez přesného engine na hodinkách.
- **Ovládání:** tlačítka vázaná na `canControlBoard` (BLE `.connected` nebo WC + `isReachable`).
- **`scenePhase`:** logování změn (staging) pro diagnostiku lifecycle vedle `onAppear`/`onDisappear`.
- **`watchWristAmbientDimming`:** na `fullScreenCover` s deskou a na `RootWatchView` — sjednocení chování při AOD.

**Velikost souboru:** monolit; údržba by v budoucnu profitovala z rozbití na menší `View` soubory (není nutné měnit nyní).

---

## 5. WatchOnboardingView + Environment

- `WatchIntroResetKey` žije ve stejném souboru jako onboarding — **používá ho jen** `ContentView` přes `@Environment(\.watchIntroReset)`; výchozí hodnota je no-op `{}`.
- Text úvodu tvrdí mimo jiné *„Stav se obnovuje i na pozadí“* — u **WC** částečně pravda (`updateApplicationContext`); u **přímého BLE** závisí na tom, zda app a BLE zůstanou aktivní (což `onDisappear` může shodit). Mírná nepřesnost vůči technické realitě.

---

## 6. WatchWristAmbientDimming

- Řeší **falešné ztmavení** při startu: `hasBeenActiveOnce` až po prvním `scenePhase == .active`.
- Overlay má `allowsHitTesting(false)` — neblokuje tapy.
- DEBUG log při změně `isDimmed` — užitečné při podezření na „zaseknuté“ UI.

---

## 7. Testy a kvalita

- **Unit testy:** prázdný `@Test` — žádná regrese na parsování FEN, `UnifiedGameState`, ani session logiku.
- **UI testy:** šablona `launch` / performance — žádné asserty na obrazovky.

Doporučení (dlouhodobě): alespoň testy shared typů a čisté funkce (`boardFromFEN`, merge stavu z JSON), které Watch používá.

---

## 8. Závislosti mimo Watch složku

- **CZECHMATEShared:** `BLEStateMachine`, `UnifiedGameState`, `BLEConnectionState`, `ChessMove`, `MoveConflictResolver`, šachovnice / FEN parser, atd.
- **Systém:** `WatchConnectivity`, `CoreBluetooth` (přes shared), `WatchKit` (haptika v modelu).

Změny chování BLE se tedy vždy projeví v iOS companion app, pokud sdílí stejný balíček.

---

## 9. Shrnutí rizik (priorita)

1. **Lifecycle** — `onDisappear` ↔ `deactivate()` a případné časté shazování BLE (řeší se logy `scenePhase` + dokumentace v `WATCH_BLE_DEEP_ANALYSIS_AND_PLAN.md`).
2. **10 s fallback na WC** — UX a očekávání uživatele vs. skutečné požadavky na iPhone v popředí.
3. **500 ms zpoždění tahu** v BLE režimu — záměr kvůli konfliktům s iPhonem; na samostatných hodinkách zbytečná latence (volitelná optimalizace).
4. **MoveConflictResolver singleton** — přežívá přes reset UI; nízká pravděpodobnost, ale stojí za povědomí při ladění „divných“ tahů.
5. **Chybějící testy** — vyšší riziko regrese při refaktorech `ContentView` / modelu.

Tento dokument lze doplňovat při každém větším zásahu do Watch targetu.
