# Zapojení ESP32-C6 ↔ STM32C031 (Hall V2)

Referenční zapojení pro profil **Hall V2** (`sdkconfig.defaults.hall_v2`) a auto-flash STM z ESP při prvním bootu.

## Topologie

```
ESP32-C6 (master)                    STM32C031 (slave / segment 0)
─────────────────                    ───────────────────────────────
GPIO12 SDA ────────────┬──────────── PB7 (I2C1_SDA, pin 1 TSSOP20)
GPIO13 SCL ────────────┼──────────── PB6 (I2C1_SCL, pin 20)
                       │
GPIO14 BOOT0_OUT ──────┼──────────── BOOT0 (pin 9)
GPIO15 NRST_OUT ───────┼──────────── NRST (pin 4)
GND ───────────────────┴──────────── GND
3V3 ──────────────────────────────── VDD
```

- **Sdílená I2C sběrnice** pro Hall čtení (`0x30`) i ROM bootloader (`0x63` při BOOT0=VDD).
- **Pull-upy** na SDA/SCL: 4,7 kΩ na 3,3 V (ESP může zapnout interní pull-up jako doplněk).
- **BOOT0**: při programování musí být **HIGH** v okamžiku náběžné hrany NRST (ESP GPIO nebo jumper na 3,3 V).
- **NRST**: aktivní LOW; ESP drží HIGH pro běh aplikace, pulz LOW→HIGH pro reset.

## I2C adresy

| Režim | 7-bit adresa | Kdy |
|-------|--------------|-----|
| Hall aplikace | `0x30` (seg0) | Po flashi, BOOT0 LOW, NRST HIGH |
| ST ROM bootloader | `0x63` | BOOT0 HIGH při uvolnění NRST (STM32C031, AN2606) |

Další segmenty (plná deska): `0x31`, `0x32`, `0x33` — každý vlastní STM + vlastní NRST GPIO.

## Kolize pinů na V1 (reed) desce

Na prototypu **V1** jsou GPIO **10** a **11** řádky reed matice. Profil `hall_v2` používá **GPIO 12/13** právě proto, aby šel ladit na samostatné testovací desce bez přemapování celé V1.

GPIO **15** je na V1 zároveň reset tlačítko — při zapnutém `CHESS_STM32_I2C_BL_ENABLE` firmware tlačítko na tomto pinu **ignoruje** (NRST řídí bootloader).

## Výrobní postup (virgin STM)

1. Sestav STM firmware: `make -C firmware/stm32_hall_c031 copy-embedded` (nebo `demo-embedded` bez HW).
2. Flash ESP včetně oddílu `stm32_fw`: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build flash`
3. První boot ESP:
   - `matrix_task` inicializuje I2C
   - `STM32_AUTO` nahraje binárku z oddílu `stm32_fw` (BOOT0 HIGH → erase → write → GO → NRST release)
   - `STM32_I2C_BL: [hall_probe] seg0 addr 0x30 OK` potvrdí běžící aplikaci
4. Matrix sken čte Hall každých 25 ms.

## Diagnostika (sériová konzole)

| Log / příkaz | Význam |
|--------------|--------|
| `STM32_AUTO: AUTO_USE_BOOT0 … BOOT0_GPIO=-1` | Chybí GPIO pro BOOT0 — virgin čip neprojde |
| `STM32_I2C_BL: GET_ID … NACK` | Špatná sběrnice, BOOT0, nebo NRST |
| `STM32_AUTO: NVS … ale Hall neodpovídá` | Vynucený reflash (výměna čipu) |
| `CLI STM32 PROBE 0 1` | Ruční vstup do ROM BL segmentu 0 |
| `CLI STM32 FLASH_PART 0 stm32_fw 1` | Ruční flash z oddílu |

## Související soubory

- `sdkconfig.defaults.hall_v2` — aktivní Kconfig profil
- `embedded/stm32_fw_embedded.bin` — image pro oddíl ESP `stm32_fw`
- `firmware/stm32_hall_c031/` — zdroj Hall firmware STM
- `components/stm32_i2c_bootloader/` — AN4221 host na ESP
- `firmware/stm32_hall_c031/Inc/hall_i2c_spec.h` — protokol Hall payload 64 B
