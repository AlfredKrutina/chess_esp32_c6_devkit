# WiFi panic: Load access fault v ieee80211_hostap_attach

## Co se děje

Při startu WiFi (APSTA) padá ESP32-C6 s:

- **Guru Meditation Error: Core 0 panic'ed (Load access fault). Exception was unhandled.**
- **MEPC/RA:** `ieee80211_hostap_attach`
- **MTVAL:** `0x00000024` (čtení z adresy 0x24 – typicky NULL + offset)
- **A0:** `0x00000000` (nulový ukazatel)

Pád je uvnitř WiFi stacku ESP-IDF (ieee80211_hostap_attach), ne v aplikačním kódu.

## Možné příčiny

1. **Pořadí vytváření netif** – na některých čipech (včetně C6) záleží, zda se vytvoří nejdřív STA nebo AP default netif. V kódu je zkoušeno pořadí: nejdřív STA, pak AP.
2. **AP+STA na ESP32-C6** – v ESP-IDF existují známé problémy s režimem APSTA na C6 (např. issue #13679). Pokud pád přetrvává, zkusit dočasně jen **WIFI_MODE_AP** (bez STA).
3. **CONFIG / verze IDF** – vyplatí se zkontrolovat `CONFIG_NEWLIB_NANO_FORMAT` (vypnuto je v pořádku) a aktualizovat ESP-IDF na nejnovější 5.5.x.

## Co v kódu zkontrolovat

- `wifi_init_apsta()` v `components/web_server_task/web_server_task.c`: pořadí `esp_netif_create_default_wifi_sta()` a `esp_netif_create_default_wifi_ap()`.
- Pokud problém zůstane: dočasně nastavit jen AP (WIFI_MODE_AP), bez STA, a ověřit, zda se pád objeví až při zapnutí STA.

## Odkazy

- [ESP-IDF Fatal Errors (ESP32-C6)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/fatal-errors.html)
- Podobný bug (C3): [IDFGH-8134](https://github.com/espressif/esp-idf/issues/9631) – souvislost s newlib/formátováním.
- ESP32-C6 AP+STA: [issue #13679](https://github.com/espressif/esp-idf/issues/13679).
