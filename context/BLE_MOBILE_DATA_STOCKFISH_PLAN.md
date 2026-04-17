# BLE + mobilní data + Stockfish — stav a ověření

**Backlog A1–E2 je v kódu hotový.** Tento soubor slouží jako **krátký referenční souhrn** a **kouřový checklist** na zařízení.

## Cíl scénáře

Uživatel má **mobilní data** (např. v autě), desku **jen přes Bluetooth**; **Stockfish / nápověda** přes internet; kde je potřeba, **`preferBluetoothOnly`** (`czechmate.preferBluetoothOnly`) zabrání přepnutí na HTTP k desce.

## Kde to v aplikaci hledat

- Přepínač *nepřepínat BLE → Wi‑Fi*: Nastavení → Připojení (+ zrcadlení ve vývojáři).
- Stockfish: síť nezávislá na desce; nápověda přes `BoardTransport` / BLE `hint_*` na desku.
- BLE reconnect / poslední deska: `BoardConnectionStore`, resume podle uloženého UUID.

## Kouřový test na fyzickém iPhonu (LTE + BLE)

1. Mobilní data pro CZECHMATE zapnutá; Wi‑Fi k domácí síti vypnutá nebo mimo dosah.
2. Bluetooth zapnutý, deska v dosahu.
3. Nastavení → Připojení → zapnout **nepřepínat na Wi‑Fi po Bluetooth**; připojit desku přes BLE.
4. Hra: živý snímek z desky.
5. Nápověda tahu: výpočet + LED na desce (dle firmware).
6. Tah + eval (pokud zapnuto), bez chyby „bez internetu“.
7. Volitelně: konzole Xcode — `[staging]` u chess-api; Low Data Mode a srozumitelný banner.

## Limity bez HTTP k desce

Část **hromadného NVS** a některá diagnostika zůstává na HTTP; **hra, nápověda LED, většina herních příkazů** jde přes BLE. Detaily byly v původním backlogu (mapování `wifiTransport` → UI šedé / BLE větev).

---

*Pro ladění Watch BLE viz `context/WATCH_BLE_DEEP_ANALYSIS_AND_PLAN.md` (sekce 6 — checkpointy).*
