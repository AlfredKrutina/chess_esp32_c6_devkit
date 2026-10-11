# Browser chess page

The human page is archived in `kos/unused/web_ui/` (`web/js/`, generated `web/chess_app.js`, piece PNGs, `chess_piece_http.c`). Firmware does not embed it and does not serve `/static/piece/*.png`.

`components/web_server_task` keeps the machine API: WiFi, `GET /` (JSON), `/api/*`, `/ws`, and OTA. `GET /chess_app.js` answers with 404 JSON so an old bookmark does not look like a crashed server.

Flutter keeps its own piece art under `flutter_czechmate/assets/pieces/` and its own openings catalog under `flutter_czechmate/assets/data/`. `tools/openings/sync_catalog.py --copy` still writes the archived browser catalog into `kos/unused/web_ui/web/`.
