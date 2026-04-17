# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "PieceBlackBishop.png.S"
  "PieceBlackKing.png.S"
  "PieceBlackKnight.png.S"
  "PieceBlackPawn.png.S"
  "PieceBlackQueen.png.S"
  "PieceBlackRook.png.S"
  "PieceWhiteBishop.png.S"
  "PieceWhiteKing.png.S"
  "PieceWhiteKnight.png.S"
  "PieceWhitePawn.png.S"
  "PieceWhiteQueen.png.S"
  "PieceWhiteRook.png.S"
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "esp-idf/esptool_py/flasher_args.json.in"
  "esp-idf/mbedtls/x509_crt_bundle"
  "esp32_chess_v24.bin"
  "esp32_chess_v24.map"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "https_server.crt.S"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "project_elf_src_esp32c6.c"
  "x509_crt_bundle.S"
  )
endif()
