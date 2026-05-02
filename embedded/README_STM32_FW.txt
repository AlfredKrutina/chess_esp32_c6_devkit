Oddíl stm32_fw (flash ESP @ 0x380000, viz partitions.csv)
============================================================

Soubor: embedded/stm32_fw_embedded.bin

- Při `idf.py flash` se tento soubor automaticky zapíše do oddílu stm32_fw (CMake: esptool_py_flash_to_partition).
- Výchozí obsah je stub z `firmware/stm32_hall_c031` (`make copy-embedded`). Pro vlastní build ho nahraď svým .bin.
- Zapojení ESP ↔ STM (BOOT0, NRST, I²C): viz context/ZAPOJENI_ESP_STM4.md

Nahrazení firmwaru STM32:
  cp /cesta/k/tvemu_firmware.bin embedded/stm32_fw_embedded.bin
  idf.py flash

Velikost oddílu je 0x80000 (512 KiB); bin může být kratší — zbytek typicky 0xFF.
