# Režim bota: pouze vizualizace tahu, uživatel pohybuje figurku

## Chování

- **Tah bota se neprovádí automaticky.** Aplikace nikdy neposílá příkaz na provedení tahu za bota (žádné volání `/api/move` ani `virtual_action` za stranu bota).
- Počítač pouze **navrhne tah** (Stockfish API) a zobrazí ho:
  - **Na webu:** podsvícení polí (od–kam) a zpráva: *„Počítač radí: zahrajte X → Y. Pohybte figurku na desce.“*
  - **Na desce:** `POST /api/game/hint_highlight` rozsvítí LED na příslušných polích.
- **Uživatel musí fyzicky pohnout figurku** na šachovnici (zvednout a položit). Tah se provede až když matrix pošle DROP a game task ho zpracuje.

## Implementace

- `playBotMove()` v `chess_app.js` volá jen `fetchStockfishBestMove()` a pak `POST /api/game/hint_highlight`.
- Stav hry na serveru (ESP32) se mění pouze na základě příkazů z matrixu (PICKUP/DROP), ne z webu za bota.
