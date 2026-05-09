# Hardwarové verze CzechMate (V1 vs V2)

Text sladí marketing (`gh-pages-ready`), README a firmware — aby nevznikal dojem, že deska na videu je tatáž jako komerční V2.

| | **V1 (prototyp v repu / na YouTube)** | **V2.0 (cílový komerční produkt)** |
|---|--------------------------------------|-------------------------------------|
| **Detekce figurek** | 8×8 **reed switch** matice — ví jen obsazeno / volné pole | **Hall senzory** — rozlišení **typu figurky** na poli (magnetické vzory), robustnější chování |
| **Formát** | Objemnější konstrukce, vývojové zapojení | Skladnější deska, zamýšlená pro sérii |
| **Firmware v tomto repu** | **`1.7.3`** — co reálně běží na prototypu (viz `CMakeLists.txt` → `PROJECT_VERSION`) | Stejný softwarový ekosystém (OTA z aplikace, API); úpravy pro Hall / STM32 segment podle integrace |
| **Video** | [YouTube představení](https://youtu.be/_MS6OP3x6Z4) — **V1** | Viz produktovka — render a mock jsou **V2** |
| **Předobjednávka / dotazníky** | Ne — reference & vývoj | Ano — zájem na webu se vztahuje k **V2** |

Technický popis multiplexu reed matice a tasků pro aktuální **V1** je v [KOMUNIKACE_MEZI_TASKY.md](KOMUNIKACE_MEZI_TASKY.md). Směr **Hall přes I2C** (`hall_i2c_matrix.h`, STM32 v `firmware/stm32_hall_c031/`) je příprava / paralelní větev pro **V2**.
