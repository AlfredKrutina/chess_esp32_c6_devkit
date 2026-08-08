# web_server_task

HTTP server, REST API, embedded web UI, and piece PNG assets.

## Structure

```
web_server_task/
├── web_server_task.c      # HTTP handler + embedded chess_app_js_content[]
├── board_api_auth.c
├── ota_update.c
├── chess_piece_http.c
├── include/
├── web/
│   ├── chess_app.js       # generated output — concat_web_js.py
│   ├── js/
│   │   ├── matrix_guard.js
│   │   ├── api.js
│   │   ├── prefs.js
│   │   └── app_main.js    # main logic (edit here)
│   └── piece_assets/      # PNGs for EMBED_FILES in CMakeLists.txt
└── tools/
    ├── concat_web_js.py   # web/js/* → chess_app.js
    ├── embed_chess_js.py  # rewrites JS array in web_server_task.c
    ├── js_to_c.py         # stdout preview of C array
    ├── update_js_in_c.py  # alternative updater
    ├── process_piece_pngs.py
    └── mqtt_panel_snippet.txt
```

Deploy web UI: [docs/reference/WEB_UI_DEPLOY.md](../../docs/reference/WEB_UI_DEPLOY.md).
