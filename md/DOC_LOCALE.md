# Documentation & source locale

Default language for docs, comments, scripts, marketing site, and firmware messages: **English**.

## Intentionally still Czech (product i18n)

| Location | Why |
|----------|-----|
| `flutter_czechmate/lib/l10n/app_cs.arb` + generated `app_localizations_cs.dart` | App Czech language pack |
| `flutter_czechmate/android/.../values-cs/` | Android Czech strings |
| Opening catalog / rationale JSON `"cs"` fields | Bilingual opening content for Czech UI |
| `_isCs()` / `cs ?` UI ternaries in opening/learn Flutter screens | Locale-aware copy |
| `scripts/docs/generate_mermaid_html.py` legacy `# ČÁST` matcher | Parses old diagram section headers |

## Renamed reference docs

- `HARDWARE_VERSIONS.md` (was `HARDWARE_VERZE.md`)
- `TASK_COMMUNICATION.md` (was `KOMUNIKACE_MEZI_TASKY.md`)
- `WIRING_ESP_STM4.md` (was `ZAPOJENI_ESP_STM4.md`)

Drop source materials in `context/`.
