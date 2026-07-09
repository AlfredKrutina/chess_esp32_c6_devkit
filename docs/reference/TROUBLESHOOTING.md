# Řešení problémů

Praktické kroky při ladění firmware, HW a webu. Úvod a build: [README.md](../../README.md). Architektura tasků: [KOMUNIKACE_MEZI_TASKY.md](KOMUNIKACE_MEZI_TASKY.md), [diagramy](../diagrams/README.md).

---

## Ladění a testy

### Debug mód

Zapnutí přes `menuconfig` nebo makro v buildu:

```c
#define CHESS_DEBUG_MODE 1
```

V `menuconfig`:

```
Component config → Chess System → Enable debug mode
```

V logu pak vidíš víc detailů o taskách a paměti.

### Test task

S zapnutým `CONFIG_CHESS_ENABLE_TEST_TASK` v monitoru:

```bash
idf.py -p /dev/ttyUSB0 monitor
# v konzoli: test
```

---

## Co občas štve (známé limity)

- RTF z Doxygen umí narůst (~10 MB) — na čtení je příjemnější HTML nebo PDF.
- Starý puzzle systém jsme vyhodili (rozhraní zůstává rozumně rozšířitelné).
- Po pádu Wi‑Fi někdy pomůže restart desky.
- Při hodně dlouhých sezeních hlídej heap / watchdog.
- Reed kontakty se mohou časem chovat hůř — čistě HW věc.

---

## Když něco nejde — první kroky

### Hardware

| Problém | Co zkontrolovat |
|---------|-----------------|
| **LED nesvítí** | Napájení 5 V pro pásek, společná zem s ESP, datový **GPIO7** na DIN |
| **Matice nic nevidí** | Zapojení reedů, pull-upy na sloupcích (~10 kΩ), řádkové výstupy |
| **Tlačítka** | Diody 1N4148 ke všem řádkům, polarita |

### Software

| Problém | Co zkontrolovat |
|---------|-----------------|
| **Zamrzlo** | Watchdog by měl resetnout — v logu poslední řádek před zásekem; zvětšit stack podezřelého tasku |
| **Šachy „nedávají smysl“** | Log tahů, příkaz `board` v UARTu, shoda matice s realitou |
| **Web nejede** | Log: `WiFi initialized …`, `Starting HTTP server` / `HTTP server started`. IP desky (STA nebo `192.168.4.1` při hotspotu), firewall z LAN, živý `web_server_task`. Starší FW při vypnutém AP mohlo skončit na `ESP_ERR_WIFI_MODE` — aktuální kód ve STA-only přeskakuje AP konfiguraci |

---

## UART — rychlá nápověda

```
help     - nápověda
move e2e4 - tah
reset    - nová hra
status   - stav hry
board    - ASCII šachovnice
test     - testy (pokud zapnutý test_task)
```

---

## OTA / aplikace

- Manifest firmware: `firmware/version.json` na `main`
- Binárka na Pages: `gh-pages/firmware/esp32_chess_v24.bin`
- Detail kanálů: [ota_architecture.md](../ota_architecture.md)
