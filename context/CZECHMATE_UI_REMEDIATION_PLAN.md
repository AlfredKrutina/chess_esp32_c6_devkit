# CZECHMATE — plán nápravy UI (mapování očekávání ↔ realita)

## Směrnice Apple (HIG) — co z toho plyne pro další práce

- **Dotykové cíle:** interaktivní prvky cílit na **min. ~44×44 pt** (včetně ikon v toolbaru); mezi sousedními akcemi dostatečný rozestup.
- **Hierarchie:** primární akce jasně odlišit (např. `borderedProminent` vs sekundární); nekombinovat „stejně důležité“ tlačítka bez vizuální priority.
- **Zpětná vazba:** po akci musí být **viditelný nebo haptický** výsledek; tiché selhání (toggle zapnutý, nic se neděje) je proti HIG.
- **Bez překryvů:** obsah nesmí konkurovat dotykům — překryvné `ZStack` vrstvy buď `allowsHitTesting(false)`, nebo přesunout mimo herní plochu.
- **Navigace:** konzistence s iOS (toolbar, sheety, `List`/`Form`); dlouhé vertikální scroll panely seskupit do sekcí s nadpisy.
- **Zdroj:** [Human Interface Guidelines](https://developer.apple.com/design/human-interface-guidelines), sekce Layout, Buttons, Materials.

---

## Fáze 0 — Inventura (label → chování)

| Oblast | Očekávání | Riziko / kontrola |
|--------|-----------|-------------------|
| Vzdálené tahy | Klepnutí od→kam na desku, HTTP/BLE | Překryvy gest, mock, bez transportu, `webLocked`, puzzle |
| Učící mód | Trenér, nápověda | Model, síť u Stockfish, synchronizace s `LearningModeManager` |
| Lampa / Wi‑Fi | Jen při HTTP + transport | Neskrývat bez vysvětlení |
| Zoom desky | Pinch / předvolby | Konflikt s tap na pole |
| Jen deska | Minimální chrome | Nesmí blokovat desku |

**Výstup fáze:** tabulka v kódu nebo wiki + 3–5 uživatelských scénářů (připojeno BLE / Wi‑Fi / mock).

---

## Fáze 1 — Kritické opravy chování (hotovo v této iteraci — část)

- [x] **Vzdálené tahy:** `ChessBoardView` — pinch a dvojité klepnutí jako `simultaneousGesture`, aby neblokovaly jednoduché klepnutí na pole.
- [x] **Důvod blokace:** `BoardConnectionStore.remoteChessFromAppBlockedReason` + `RemoteMovesHintBanner` ve standardním layoutu, panelu „Více“, širokém panelu, sheetu ovládání, režimu jen deska (pasivní banner).
- [x] **Standard:** `GameStatusSummary` pod šachovnicí (ne přes ni) — žádné nechtěné překryvy a lepší dostupnost polí.
- [x] **Jen deska:** odstraněn celoplošný `Color.clear` se záchytem dotyků; přehled jen horní/spodní pruh + tlačítko oko pro obnovení.

---

## Fáze 2 — Konzistence přepínačů a stavu

- [x] Sjednotit texty: `GameRemoteMovesCopy` (`toggleTitle` + `usageHint`) ve `GameView`, sheetu, wide panelu, `GameControlPanel`, VoiceOver u banneru.
- [x] Po neúspěšném vzdáleném tahu: `TransientBoardMessageBanner` + `onRemoteMoveRequestFailed` v `ChessBoardContainer`; legacy `GameView` transient pod deskou.
- [x] `AppStorage` ↔ `displayOptions`: při `scenePhase == .active` znovu načíst předvolby z úložiště (`GameContainerCoachLifecycleModifier`).

---

## Fáze 3 — Hustota layoutu a iPad

- [x] Široký režim: sidebar — stav, akce, trenér, **`BoardLampQuickStrip`**, nastavení zobrazení, historie; vlevo deska + stav + transient (bez lampy pod deskou).
- [x] Standardní telefon: pod deskou remote banner → **ovládací panel** → trenér → lampa → historie (regular).
- iPad split: minimální šířka sidebaru, případně collapsible sekce.
- Tab bar: max. 5 položek, jasné ikony (již řešeno dříve — udržovat).

---

## Fáze 4 — Přístupnost a lokalizace

- [x] VoiceOver u tahů z aplikace: sloučený hint (`usageHint` + případná blokace); `ToggleOption` — `isToggle` + „Zapnuto/Vypnuto“.
- [x] Toolbar: `ThemeToolbarIconChip` a `LayoutModePicker` — cíl klepnutí cca **44×44 pt** (HIG).
- Dynamic Type: ověřit `caption`/`footnote` u bannerů.
- Barevný kontrast bannerů (oranžová výstraha vs `secondary` text).

---

## Fáze 5 — QA checklist

- iPhone SE, standardní iPhone, iPad, landscape.
- Scénáře: mock, BLE bez Wi‑Fi, Wi‑Fi, `webLocked`, puzzle.
- Regrese: nápověda, undo, nová hra, coach chat.

---

*Poslední úprava: fáze 2–4 dle plánu (transient zprávy, wide layout, a11y toolbar, legacy copy).*
