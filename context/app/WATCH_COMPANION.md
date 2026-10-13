# Apple Watch a Live Activity (CzechMate)

Hodinky jsou doprovod iPhonu u fyzické desky: ukazují čas a stav, nehrají partii.

## Obrazovky

| Kde | Obsah |
|---|---|
| Watch app, strana 1 | Oba hráči pod sebou (Černý nahoře, Bílý dole), velké číslice, zvýrazněná strana na tahu, Málo času / Čas vypršel. Společný stav (Pauza, deska nepřipojena, iPhone mimo dosah) jedním řádkem nad hodinami. |
| Watch app, strana 2 „U stolu“ | Spojení s deskou (Připojena / Připojuji / Nepřipojena + Bluetooth / Wi‑Fi), iPhone mimo dosah, časová kontrola a tahy, tlačítko Pauza / Pokračovat s potvrzením. |
| Live Activity — zamčená obrazovka | Dvě plotny vedle sebe, běžící strana odpočítává systémovým časovačem. |
| Dynamic Island | Rozbalené: oba časy + stav. Kompaktní: značka strany na tahu + její čas. Minimální: běžící čas. |
| Smart Stack na Watch (iOS 18+) | Vlastní layout `.small`: dva řádky, strana na tahu zvýrazněná. Na iOS 16.2–17 systém skládá kompaktní pohled. |

Na hodinky nepatří: tah, remíza, vzdání, deska s figurkami, puzzle, analýza, nastavení, Foundry, průvodce, párování.

## Payload (Flutter → iOS)

`LiveActivityService._buildPayload` posílá stejnou mapu do ActivityKit i do WatchConnectivity:

| Klíč | Význam |
|---|---|
| `phase` | `no_game` / `no_timer` (bez časové kontroly) / `clock` |
| `whiteTimeMs`, `blackTimeMs`, `isWhiteTurn`, `timerRunning`, `gamePaused`, `timeExpired`, `gameFinished`, `totalMoves`, `timeControlLabel`, `transport` | Stav hodin z desky |
| `lowTimeMs` | Práh „Málo času“ (`lowTimeThresholdMs`, 10 % základu, 10–60 s) |
| `clockAnchorMs` | Epoch ms, kdy se hodnoty hodin naposledy změnily; od něj běžící strana odpočítává |
| `link` | `live` / `connecting` / `offline` (`connectionTierOf`, stejně jako herní obrazovka) |
| `lang` | `cs` / `en` podle Nastavení → Jazyk; bez něj platí jazyk zařízení |

`ChessLiveAttributes.ContentState` existuje ve dvou kopiích (`ios/Runner/` a `ios/ChessLiveActivityExtension/`), musí zůstat shodné.

Pravidla zobrazení jsou v `ios/Shared/CzechMateClockKit.swift` (sdílí Live Activity extension i watch extension) a odpovídají `resolveClockFace` ve Flutteru: vlajka jen když ji hlásí deska, lokální odpočet nikdy sám nevlajkuje.

## Pauza z hodinek

- Watch posílá `pause` / `resume` jen přes `WCSession.sendMessage` (živě). Fronta `transferUserInfo` se pro příkazy nepoužívá, aby pauza nedorazila o minuty později; `WatchBridge` takové příkazy ignoruje.
- `WatchBridge` čeká na výsledek z Flutteru; `watch_command_binding.dart` vrací `true` jen když deska příkaz přijala a hodiny jde přepnout (stejná podmínka jako tlačítko Pauza na iPhonu).
- Hodinky ukážou „Odesláno na desku“ nebo „Nepovedlo se. Použij iPhone.“

## Staging na simulátoru

Debug build watch app přijímá launch argumenty bez spárovaného iPhonu:

```
xcrun simctl launch <watch-sim> com.example.flutterCzechmate.watchkitapp -cmDemo low -cmLang cs -cmPage 0
```

`-cmDemo`: `clock`, `low`, `paused`, `offline`, `nophone`, `flag`, `nogame`. `-cmPage 1` otevře „U stolu“.
