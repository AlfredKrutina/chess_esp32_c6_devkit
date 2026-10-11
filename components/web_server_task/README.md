# web_server_task

WiFi (AP/STA), HTTP machine API, WebSocket `/ws`, OTA, and the BLE JSON bridge. Flutter and the Windows client call this component. They do not open a browser page on the board.

`GET /` returns a short JSON document (`service`, `api`, `websocket`). `GET /chess_app.js` returns 404 JSON. Piece PNGs are not embedded.

The human chess page (JS, CSS-less modules, piece PNGs, and the scripts that packed them into firmware) is in `kos/unused/web_ui/`. It is not compiled and not served.

`tools/mqtt_panel_snippet.txt` stays here. It is a Home Assistant MQTT panel snippet for `ha_light_task`, not the browser chess page.
