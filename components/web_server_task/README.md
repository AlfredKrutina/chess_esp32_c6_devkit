# web_server_task

WiFi (AP/STA), HTTP machine API, WebSocket `/ws`, OTA, and the BLE JSON bridge. Flutter and the Windows client call this component. They do not open a browser page on the board.

`GET /` returns a short JSON document (`service`, `api`, `websocket`). `GET /chess_app.js` returns 404 JSON. Piece PNGs are not embedded.

The human chess page (JS, CSS-less modules, piece PNGs, and the scripts that packed them into firmware) is in `kos/unused/web_ui/`. It is not compiled and not served.

`tools/mqtt_panel_snippet.txt` stays here. It is a Home Assistant MQTT panel snippet for `ha_light_task`, not the browser chess page.

## Command token, puzzle, resign

Game writes (`POST /api/move`, a command frame on `/ws`) require `Authorization: Bearer <64 hex>` from pairing. The token is issued on BLE `cmd_ack` after a new bond, or by `POST /api/pair` while the 60 s pairing window is open. Up to three device slots, one commander. A different valid token gets `board_busy`. Snapshot GET does not use this token.

| Method | Path | Body |
|--------|------|------|
| POST | `/api/pair` | empty; only while the pairing window is open |
| POST | `/api/game/puzzle` | `{"action":"load","fen":"…","solution":["e2e4",…]}`, or `start`, `cancel` (`prepare` and `hint` still accepted). At most 12 plies. `start` on a mismatched matrix is 409 `physical_mismatch`. |
| POST | `/api/game/resign` | `{"side":"white"}` or `{"side":"black"}`. 409 when no game is running, or puzzle / opening / setup owns the board, or the side is not on move. |

BLE uses the same JSON with `"cmd":"puzzle"` or `"cmd":"resign"`. Czech behaviour note: [context/app/deska-a-aplikace.md](../../context/app/deska-a-aplikace.md).
