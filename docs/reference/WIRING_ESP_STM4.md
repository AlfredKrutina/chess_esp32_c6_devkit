# ESP32-C6 ↔ STM32C031 wiring (Hall V2)

Reference wiring for the **Hall V2** profile (`sdkconfig.defaults.hall_v2`) and auto-flash of STM from ESP on first boot.

## Topology

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

- **Shared I2C bus** for Hall reads (`0x30`) and ROM bootloader (`0x63` when BOOT0=VDD).
- **Pull-ups** on SDA/SCL: 4.7 kΩ to 3.3 V (ESP can enable internal pull-ups as supplement).
- **BOOT0**: for programming must be **HIGH** at NRST rising edge (ESP GPIO or jumper to 3.3 V).
- **NRST**: active LOW; ESP holds HIGH for app run, LOW→HIGH pulse for reset.

## I2C addresses

| Mode | 7-bit address | When |
|-------|--------------|-----|
| Hall application | `0x30` (seg0) | After flash, BOOT0 LOW, NRST HIGH |
| ST ROM bootloader | `0x63` | BOOT0 HIGH on NRST release (STM32C031, AN2606) |

Additional segments (full board): `0x31`, `0x32`, `0x33` — each own STM + own NRST GPIO.

## Hall pair geometry

- **2 Hall sensors per square** (differential pair → firmware DIFF `|r0 − r1|`).
- **Center-to-center pitch within one square: 6.267 mm.**

See also [HARDWARE_VERSIONS.md](HARDWARE_VERSIONS.md) (V2 Hall geometry).

## Pin conflicts on V1 (reed) board

On **V1** prototype GPIO **10** and **11** are reed matrix rows. Profile `hall_v2` uses **GPIO 12/13** so Hall can be debugged on a separate test board without remapping all of V1.

GPIO **15** on V1 is also the reset button — with `CHESS_STM32_I2C_BL_ENABLE` firmware **ignores** the button on that pin (NRST is driven by the bootloader).

## Manufacturing flow (virgin STM)

1. Build STM firmware: `make -C firmware/stm32_hall_c031 copy-embedded` (or `demo-embedded` without HW).
2. Flash ESP including `stm32_fw` partition: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build flash`
3. First ESP boot:
   - `matrix_task` initializes I2C
   - `STM32_AUTO` uploads binary from `stm32_fw` partition (BOOT0 HIGH → erase → write → GO)
   - If GO fails: **NRST pulse with BOOT0 LOW** (start app from flash)
   - `STM32_I2C_BL: [hall_probe] seg0 addr 0x30 OK` confirms running application
4. Matrix scan reads Hall every 25 ms.

## Diagnostics (serial console)

| Log / command | Meaning |
|--------------|--------|
| `STM32_AUTO: AUTO_USE_BOOT0 … BOOT0_GPIO=-1` | Missing BOOT0 GPIO — virgin chip will not pass |
| `STM32_I2C_BL: GET_ID … NACK` | Bad bus, BOOT0, or NRST |
| `STM32_AUTO: NVS … but Hall not responding` | Forced reflash (chip swap) |
| `CLI STM32 PROBE 0 1` | Manual entry to ROM BL segment 0 |
| `CLI STM32 FLASH_PART 0 stm32_fw 1` | Manual flash from partition |
| `CLI HALL PROBE 0` | Verify Hall app after flash |

## Related files

- `sdkconfig.defaults.hall_v2` — active Kconfig profile
- `embedded/stm32_fw_embedded.bin` — image for ESP `stm32_fw` partition
- `firmware/stm32_hall_c031/` — Hall firmware source for STM
- `components/stm32_i2c_bootloader/` — AN4221 host on ESP
- `firmware/stm32_hall_c031/Inc/hall_i2c_spec.h` — Hall payload protocol 64 B
