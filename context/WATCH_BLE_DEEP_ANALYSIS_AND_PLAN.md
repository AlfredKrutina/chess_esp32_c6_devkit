# Watch + BLE: hluboká analýza příčin „stále to nejde“ a plán řešení

Dokument slouží jako kontext pro ladění: rozlišuje **prokázané chyby v kódu**, **pravděpodobné systémové příčiny** a **ověření na zařízení / firmwaru**.

---

## 1. Prokázané / opravené problémy v aplikaci

### 1.1 Falešné „Bluetooth vypnutý“ při prvním skenu (kritické)

**Příčina:** `startScanning()` kontroloval `central.state` hned po vytvoření `CBCentralManager`. Stav je často `.unknown` nebo `.resetting`. Původní logika mapovala vše kromě `.unauthorized` na **`.bluetoothOff`**.

**Důsledek:** První odložený sken (+0,5 s z `WatchUnifiedSessionModel`) často **nikdy nespustil** `scanForPeripherals`, UI ukázalo špatný stav a uživatel měl dojem, že BLE „nefunguje“.

**Řešení v kódu:** Rozlišení stavů (`unknown` / `resetting` ≠ vypnuto), případ `.unsupported`, a **odložený sken** po přechodu na `.poweredOn` přes příznak `deferStartScanUntilPoweredOn` (nastavený jen z `bleCallbackQueue`).

**Soubor:** `CZECHMATEShared/.../BLEStateMachine.swift` — `startScanning`, `centralManagerDidUpdateState`.

### 1.2 Hlavní vlákno + CoreBluetooth (dříve)

**Příčina:** `CBCentralManager` na main queue + práce v `Task { @MainActor }` uvnitř delegate callbacků.

**Důsledek:** Na watchOS mohlo dojít k **hladovění UI** (SwiftUI se „nenačte“, zatímco log ukazuje `activate() hotovo`).

**Stav:** Řešeno dedikovanou `bleCallbackQueue` a oddělením UI aktualizací na MainActor.

### 1.3 Závod `activate()` vs. `startScanning()`

**Příčina:** Asynchronní vytvoření manageru vs. brzké `startScanning`.

**Stav:** Řešeno `bleCallbackQueue.sync` v `activate()`.

### 1.4 Ostatní dřívější úpravy

- `disconnect()` a návrat do `.idle` (kromě `bluetoothOff` / `unauthorized`).
- `didConnect`: `lastConnectedPeripheralIdentifier` z `peripheral.identifier` na delegate vlákně.

---

## 2. Pravděpodobné příčiny mimo jednoduchou logiku skenu

### 2.1 Životní cyklus SwiftUI (`onDisappear`)

`ContentView` volá `model.deactivate()` v `.onDisappear`.

- Na watchOS závisí na tom, **kdy** systém považuje view za „zmizelé“ (TabView, přechod do pozadí, úvodní obrazovka).
- Pokud se `deactivate()` volá častěji, než uživatel čeká, BLE se **tears down** a spojení padá.

**Plán ověření:** Logovat `onDisappear` / `deactivate` spolu s časem a scénou; zvážit `scenePhase` místo nebo vedle `onDisappear` pro řízení BLE (např. neukončovat při krátkém `.inactive`, pokud je to žádoucí).

### 2.2 Režim „po 10 s na WatchConnectivity“

`attemptBLEConnection()` po 10 s bez připojení přepne na `.watchConnectivity` a **vypne** BLE state machine.

- Pokud byl problém jen **opožděný** první sken (nyní částečně řešeno), uživatel skončí v WC režimu.
- V WC režimu **`canControlBoard`** vyžaduje `isReachable` (iPhone app v popředí) pro většinu příkazů → dojem „nic nejde“.

**Plán:** Po opravě skenu znovu otestovat; případně prodloužit timeout nebo zobrazit jasný banner „přepnuto na iPhone — otevři app“.

### 2.3 Konkurence a izolace (`sendCommand` / chyby)

Část kódu volá `delegate?.stateMachine(..., didEncounterError:)` z cesty, která může běžet mimo MainActor. Na přísnějším Swift 6 / budoucích verzích to může být problém.

**Plán:** Sjednotit hlášení chyb přes `Task { @MainActor in ... }`.

### 2.4 `connected` před dokončením notifikací

Po objevení charakteristik se nastaví `.connected` a volá se `setNotifyValue(true, ...)`. Snapshot může přijít až po zapnutí notify.

**Plán:** Pokud UI očekává data okamžitě, zvážit stav „připojeno, čekám na první snapshot“ nebo jednorázový `read` snapshot charakteristiky (pokirm to firmware podporuje konzistentně).

### 2.5 Neukončený `readNetworkContinuation`

Při odpojení během `readNetworkInfo()` může zůstat viset continuation.

**Plán:** Při `disconnect` / `clearBleReferences` resume s chybou nebo zrušit operaci.

---

## 3. Firmware a rádiové vrstvy (nutné ověřit na desce)

### 3.1 UUID v reklamě

V `components/ble_task/ble_nimble_impl.c` je vysvětleno little-endian pro `BLE_UUID128_INIT` a služba je v **primárním advertising paketu**. iOS filtr `scanForPeripherals(withServices: [serviceUUID])` vyžaduje, aby UUID bylo v reklamě — to odpovídá aktuálnímu ESP kódu.

**Riziko:** Starý firmware s big-endian UUID → telefon/hodinky **nikdy nenajdou** periferii. Ověřit verzi firmware na desce.

### 3.2 Dosah, rušení, jedna aktivní BLE role

Watch a iPhone současně jako central k jedné desce, nebo ESP v jiném režimu — může ovlivnit chování.

**Plán:** Testovat **jen Watch ↔ deska** (iPhone BT vypnutý nebo app zavřená), pak kombinace.

---

## 4. Plán práce „až to uděláš reálně“ (pořadí)

1. **Nasadit build** s opravou `CBManagerState` + odloženým skenem; na hodinkách zapnout **staging logy** (`WatchAppLog` / konzole Xcode).
2. **Scénář A — cold start:** spustit app, sledovat pořadí: `activate` → stav CB (`unknown` → `poweredOn`) → `startScanning` → `.scanning` → případně `didDiscover`.
3. **Scénář B — žádná deska v dosahu:** UI musí zůstat v `.scanning` nebo `.idle`, **ne** falešné „BT vypnutý“.
4. **Scénář C — deska zapnutá:** ověřit spojení a první snapshot do `UnifiedGameState`.
5. **Scénář D — pozadí / listování TabView:** zkontrolovat, zda se nevolá `deactivate` příliš často (logy).
6. **Scénář E — WC fallback:** po 10 s bez BLE ověřit, že uživatel rozumí, že potřebuje iPhone v popředí pro příkazy.
7. **Volitelně:** opravy z bodů 2.3–2.5 podle výsledků profilování a crashů.

---

## 5. Shrnutí

Nejpravděpodobnější **skutečná aplikační chyba**, která přetrvávala i po přesunu BLE z main threadu, byla **špatná interpretace `CBManagerState.unknown` jako vypnutého Bluetooth**, kvůli čemuž se první sken často vůbec nespustil. Další vrstva problémů je **UX režimu WatchConnectivity** a **životní cyklus** `activate`/`deactivate` na watchOS.

Tento dokument a opravy ve `BLEStateMachine` by měly být první linie; pokud potíže přetrvají, následuje systematické logování cyklu view a BLE stavů podle sekce 4.

---

## 6. Postupný průzkum (checkpointy)

Pracujeme **krok za krokem**; každý krok má očekávané logy (DEBUG build, konzole Xcode → Watch proces).

### Krok 1 — Cold start a CoreBluetooth

**Co udělat:** Spustit Watch app z Xcode, sledovat výstup.

**Co hledat (pořadí):**

| Zdroj | Řádek / prefix | Význam |
|--------|-----------------|--------|
| Watch | `[Watch][STAGING] ContentView onAppear` | UI běží, jde se do `activate()` |
| Watch | `[Watch][BLE] attemptBLEConnection` / `odložené startScanning` | Session model +0,5 s sken |
| Shared | `[BLEStateMachine][staging] activate: manager vytvořen` | `CBState=` hned po init (často `0` = unknown) |
| Shared | `startScanning: CBState=` | První nebo odložený pokus |
| Shared | buď `scanForPeripherals` nebo `odloženo do poweredOn` | Správná větev místo falešného „poweredOff“ |
| Shared | `centralDidUpdateState: CBState=5` | `5` = poweredOn (watchOS/iOS rawValue) |
| Shared | `runDeferredScan=true` + `opakuji startScanning` | Doplnění skenu po přechodu z unknown |

**Rozhodnutí:** Pokud po `poweredOn` není `didDiscover` a deska je zapnutá v dosahu → **Krok 3** (firmware / reklama). Pokud vidíš časté `deactivate` bez očekávání → **Krok 2**.

### Krok 2 — Životní cyklus UI (`scenePhase` vs `onDisappear`)

**Co přidáno:** `ContentView` loguje `scenePhase → …` při každé změně.

**Co hledat:** Zda se `onDisappear — deactivate()` objeví při **jen** `.inactive` / přepnutí TabView / spuštění fullScreen desky. Pokud ano, BLE se zbytečně shazuje.

**Další akce (až po potvrzení logy):** Upravit strategii — např. `deactivate` jen při `.background` nebo při opuštění celé app scény (nutná úprava kódu).

### Krok 3 — Deska v dosahu, žádný `didDiscover`

- Ověřit **verzi firmware** na ESP (UUID v reklamě — `ble_nimble_impl.c`).
- Zkusit **jen Watch ↔ deska** (iPhone BT vypnutý), případně naopak.
- V logu musí přijít `[BLEStateMachine][staging] didDiscover: …` — pokud ne, problém je spíš **rádiová vrstva / advertising**, ne SwiftUI.

### Krok 4 — Po `didDiscover` a dál

- Spojení: `didConnect` → služby → charakteristiky → `stav BLE: Připojeno` z `WatchUnifiedSessionModel`.
- Pokud spojení spadne: logovat `didFailToConnect` / chyby GATT (lze doplnit v další iteraci).

### Krok 5 — Fallback WatchConnectivity (10 s)

- Log `[Watch][STAGING] BLE bez připojení po 10s → přepínám na WatchConnectivity`.
- Pak očekávej potřebu **otevřené iPhone app** pro příkazy (`isReachable`).

---

**Poznámka:** Řádky `[BLEStateMachine][staging]` jsou v **DEBUG** buildu v `BLEStateMachine.swift`; Release je vyřeže (prázdná funkce).

Implementované opravy Watch/BLE (`CBManagerState`, defer sken, fronty, idempotentní `activate`, zrušitelné odložené skeny, lifecycle `scenePhase`, continuation u `readNetwork`, atd.) jsou **přímo v repu** — tento dokument je návod k ladění, ne duplicitní checklist.
