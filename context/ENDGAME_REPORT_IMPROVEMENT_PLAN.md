# Plán: souhrn partie (Endgame report)

## Problém: sheet se objevoval a mizel

### Příčiny (analytika)

1. **Klíč auto-otevření obsahoval `state_version`** — při každém HTTP pollu se verze zvedla, klíč se změnil, uživatel už měl souhrn zavřený (`dismissed` pro starý klíč), takže se sheet **znovu otevřel**.
2. **Při `snapshot == nil`** (výpadek / mezistav) klíč byl `nil` a logika resetovala `showEndgameReport` → **souhrn se zavřel**.
3. **Obsah sheetu byl vázaný jen na `store.snapshot`** — při krátké ztrátě dat se zobrazil prázdný stav i při otevřeném sheetu.

### Implementovaná náprava

- Klíč konce partie: `GameEndReportSessionKey` — **`game_id`** z JSON (`game_id`) pokud je k dispozici, jinak `move_count` + signatura desky (ne `state_version`).
- Při `newKey == nil` resetovat UI jen pokud existuje snapshot a partie **není** ukončená.
- V `GameEndReportSheet`: **latch** `GameSnapshot` při `onAppear`, `onDisappear` vyčistit; staging log při zamčení.
- **`@AppStorage("czechmate.endgameReportAutoOpen")`** — vypnutím se vypne jen **automatické** otevření; ruční „Souhrn“ zůstává. Přepínač v **Nastavení → Hra a UI**.

### Volitelné / neřešeno

- **PDF / obrázek grafu** — zatím ne (sdílení textu + PGN).
- **Materiál v čase** — vyžaduje replay nebo historii braní.
- **Plná SwiftData historie** — nahrazeno lehkým **UserDefaults** (poslední nadpis + fingerprint) pro banner „Předchozí partie“.

---

## Grafy a průběh

### Data z desky

- **Odehraný čas** — křivka součtu (min) z rozdílů `timestamp`.
- **Čas na tah** — sloupce (s), B/Č, horizontální scroll.

### Časomíra (`snapshot.clock`)

- Pokud je v snímku `clock` a `config.type != 0`, **sloupce zbývajícího času** (min) Bílý / Černý na konci partie.

### Evaluace (Stockfish)

- **`BoardConnectionStore.moveEvalHistory`** — při zapnutém vyhodnocování tahů se po každém tahu ukládá eval z perspektivy bílého + `MoveGrade` + ztráta v pěšcích.
- U **nejlepšího tahu** je navíc druhý Stockfish dotaz na `fenAfter` (stejně jako u ostatních) — o něco vyšší náklad při perfektní hře.
- V souhrnu: **čára + body** podle kvality tahu.

### Export

- **PGN** v textu sdílení + samostatná položka menu **Sdílet PGN**.
- Implementace: `GameHistoryPGN.swift`.

### UX / lokalizace

- **`.presentationDetents([.large, .medium])`** + táhlo.
- **EN:** pokud `Locale.preferredLanguages` začíná na `en`, většina popisků v sheetu přes `EndgameReportCopy` (výsledek partie zůstává CS z modelu).

---

## Testy (`CZECHMATETests/EndgameReportTests.swift`)

- Časové řady (`GameEndReportTiming`).
- Klíč session (`GameEndReportSessionKey`) včetně `game_id`.
- PGN obsahuje `[Result]` a tahy.

---

## Související soubory

- `GameContainerView.swift` — auto-open, `AppStorage`, klíč.
- `GameEndReportSheet.swift`, `GameEndReportSessionKey.swift`, `GameEndReportTiming.swift`, `GameHistoryPGN.swift`, `EndgameReportCopy.swift`
- `BoardConnectionStore.swift` — `moveEvalHistory`, `runMoveEvaluation`.
- `GameSnapshot.swift` — volitelné `game_id`.
- `SettingsTabView.swift` — přepínač auto-souhrnu.
- `MoveEvalHistoryEntry.swift`
