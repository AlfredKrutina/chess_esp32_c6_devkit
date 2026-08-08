stm32_fw partition (ESP flash @ 0x380000, see partitions.csv)
============================================================

File: embedded/stm32_fw_embedded.bin

- On `idf.py flash`, this file is written automatically to the stm32_fw partition (CMake: esptool_py_flash_to_partition).
- Default content is a stub from `firmware/stm32_hall_c031` (`make copy-embedded`). Replace with your own .bin for a custom build.
- ESP ↔ STM wiring (BOOT0, NRST, I²C): see docs/reference/WIRING_ESP_STM4.md

Replace STM32 firmware:
  cp /path/to/your_firmware.bin embedded/stm32_fw_embedded.bin
  idf.py flash

Demo (synthetic Hall data over I²C, same protocol as production — no ADC/mux on STM):
  cd firmware/stm32_hall_c031 && make demo-embedded
  → copies build/stm32_hall_c031.bin to embedded/stm32_fw_embedded.bin ; then idf.py flash

Partition size is 0x80000 (512 KiB); the bin may be shorter — remainder is typically 0xFF.
