# watchOS — samostatné připojení (šablona)

Aplikace na iPhone používá `BoardDiscoveryView` a `BLEBoardTransport` / `LANBoardDiscoveryService`. Pro **hodinky bez iPhonu** přidej v Xcode cíl **Watch App** a zkopíruj sem uvedené soubory do targetu hodinek.

- `WatchCZECHMATEApp.swift` — `@main`
- `WatchContentView.swift` — jednoduchý stav partie + tlačítko „Najít desku“ (BLE scan)
- `WatchNetworkMonitor.swift` — `NWPathMonitor` pro banner „Stockfish potřebuje internet“
- Sdílené UUID: stejné jako `CZECHMATE/CZECHMATE/Services/CZECHMATEBLEUUIDs.swift` (přidej soubor do obou targetů nebo vytvoř sdílený Swift package).

Stockfish volání z hodinek: použij `URLSession` na stejný chess-api endpoint jako na iOS (telefon musí mít buď Wi‑Fi/LTE na hodinkách).

**Poznámka:** Instalace watch aplikace z App Store vyžaduje iPhone jednou; běžné hraní jen s hodinkami = deska + BLE + síť pro API.
