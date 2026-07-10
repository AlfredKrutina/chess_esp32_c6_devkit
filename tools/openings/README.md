# Opening catalog tools

## sync_catalog.py

```bash
pip install jsonschema python-chess   # optional but recommended for CI
python3 tools/openings/sync_catalog.py --validate --physical-rules
python3 tools/openings/sync_catalog.py --validate --physical-rules --copy
```

- **Source of truth:** `data/openings_master.json`
- **Schema:** `data/openings_catalog.schema.json`
- **Outputs:** `components/web_server_task/web/data/openings_catalog.json`, `flutter_czechmate/assets/data/openings_catalog.json`
