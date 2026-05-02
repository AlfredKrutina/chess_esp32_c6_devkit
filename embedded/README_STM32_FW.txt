Oddíl stm32_fw (flash ESP @ 0x380000, viz partitions.csv)
============================================================

Soubor: embedded/stm32_fw_embedded.bin

- Při `idf.py flash` se tento soubor automaticky zapíše do oddílu stm32_fw (stejně jako factory app).
- Placeholder v repu je záměrně krátký (eff < 32 B užitečných dat) — ESP auto-flash STM přeskočí,
  dokud sem nezkopíruješ skutečný výstup z STM32 (např. project.hex převedený na .bin, nebo přímo .bin).

Nahrazení firmwaru STM32:
  cp /cesta/k/tvemu_firmware.bin embedded/stm32_fw_embedded.bin
  idf.py flash

Velikost oddílu je 0x80000 (512 KiB); bin může být kratší — zbytek typicky 0xFF.
