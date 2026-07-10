# Opening catalog tools

## sync_catalog.py

```bash
pip install jsonschema python-chess   # optional but recommended for CI
python3 tools/openings/sync_catalog.py --validate --physical-rules --mirror-symmetric
python3 tools/openings/sync_catalog.py --validate --physical-rules --mirror-symmetric --copy
```

- **Source of truth:** `data/openings_master.json`
- **Schema:** `data/openings_catalog.schema.json`
- **Outputs:** `components/web_server_task/web/data/openings_catalog.json`, `flutter_czechmate/assets/data/openings_catalog.json`, `components/web_server_task/web/js/opening_catalog.js` (generated on `--copy`)

## pgn_to_catalog.py

```bash
python3 tools/openings/pgn_to_catalog.py --pgn studies/italian.pgn --id italian_giuoco_white --side white
```

Extracts UCI plies from the first game in a PGN file and prints a JSON draft for manual merge into `openings_master.json`.
