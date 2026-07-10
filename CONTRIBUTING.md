# Contributing to CZECHMATE

## Build firmware

```bash
source "$IDF_PATH/export.sh"
idf.py set-target esp32c6
idf.py build
```

Or `./scripts/idf_build.sh` from a shell with ESP-IDF loaded.

## Flutter client

```bash
cd flutter_czechmate
flutter pub get
flutter test
```

## Localization (ARB)

- Edit `flutter_czechmate/lib/l10n/app_en.arb` and `app_cs.arb`.
- Do **not** run `generate_arb.py` blindly — it can overwrite the full key set.
- Regenerate `app_localizations*.dart` via `flutter gen-l10n` or project tooling after ARB changes.

## Pull requests

- One logical change per PR when possible (move-only refactors separate from behavior fixes).
- CI must pass: firmware build, Flutter test (when touching client), docs diagrams (when touching Mermaid sources).
- See [docs/reference/PROFESSIONAL_CLEANUP_PLAN.md](docs/reference/PROFESSIONAL_CLEANUP_PLAN.md) for the long-term refactor roadmap.

## Branch naming (agents / automation)

Use `cursor/<descriptive-name>-8fdd` for automated branches.
