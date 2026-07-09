# web_server_task

HTTP server, REST API, embedded web UI a PNG figurek.

## Struktura

```
web_server_task/
├── web_server_task.c      # HTTP handler + embedded chess_app_js_content[]
├── board_api_auth.c
├── ota_update.c
├── chess_piece_http.c
├── include/
├── web/
│   ├── chess_app.js       # zdroj web UI — po úpravě spusť tools/embed_chess_js.py
│   └── piece_assets/      # PNG pro EMBED_FILES v CMakeLists.txt
└── tools/
    ├── embed_chess_js.py  # přepíše JS pole v web_server_task.c
    ├── js_to_c.py         # stdout náhled C pole
    ├── update_js_in_c.py  # alternativní updater
    ├── process_piece_pngs.py
    └── mqtt_panel_snippet.txt
```

Deploy web UI: [docs/reference/WEB_UI_DEPLOY.md](../../docs/reference/WEB_UI_DEPLOY.md).
