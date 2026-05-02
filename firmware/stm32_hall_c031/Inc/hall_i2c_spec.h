/**
 * @file hall_i2c_spec.h
 * @brief Norma I2C mezi ESP32 (master) a 4× STM32 — 128 Hall kanálů → 64 polí.
 *
 * Zkopíruj tento soubor do STM32 projektu (bez závislosti na ESP-IDF).
 *
 * === Topologie ===
 * - 4 stejné segmenty (plošňáky), každý jeden I2C slave (vlastní 7bit adresa).
 * - Na segmentu 16 šachových polí × 2 Hall senzory = 32 ADC kanálů → 32× uint16.
 * - ESP čte postupně všechny 4 adresy každý scan cyklus.
 *
 * === I2C transakce (povinná pro ESP) ===
 * - Master: START + SlaveAddr(W) + 1 byte „pointer register“ + REPSTART +
 *   SlaveAddr(R) + read 64 bytes + STOP.
 * - Pointer 0x00 = blok surových vzorků (viz níže).
 * - Formát čísel: little-endian (nízký bajt první).
 *
 * === Registr / pointer mapa ===
 * Hodnoty jsou stejné jako výchozí CONFIG_CHESS_HALL_REG_START v Kconfig.
 */
#pragma once

#include <stdint.h>

#define HALL_I2C_REG_POINTER_RAW 0x00u

/** Volitelné na STM: po přečtení 1 B master ověří kompatibilitu (ESP zatím nečte). */
#define HALL_I2C_REG_POINTER_PROTO_VER 0x01u
#define HALL_I2C_PROTOCOL_VERSION 1u

/** Počet polí na jednom segmentu (čtvrtina šachovnice). */
#define HALL_I2C_FIELDS_PER_SEGMENT 16u
/** Hall senzorů na pole (diferenciální pár / dvojice kanálů muxu). */
#define HALL_I2C_SENSORS_PER_FIELD 2u
/** uint16 vzorků na segment (16 × 2). */
#define HALL_I2C_UINT16_PER_SEGMENT                                                \
  (HALL_I2C_FIELDS_PER_SEGMENT * HALL_I2C_SENSORS_PER_FIELD)
/** Bajtů přenášených při čtení pointeru 0x00 (32 × 2). */
#define HALL_I2C_PAYLOAD_BYTES (HALL_I2C_UINT16_PER_SEGMENT * sizeof(uint16_t))

/**
 * === Rozložení payloadu 0x00 (64 B) ===
 * Offset v bajtech: field 0..15, sensor 0..1
 *   off = field * 4 + sensor * 2
 *   uint16_t v = buf[off] | (buf[off + 1] << 8);
 *
 * Logický kanál muxu: sample_index = field * 2 + sensor  (0..31).
 * „Kanály 0+1 = pole 0, 2+3 = pole 1“ odpovídá field = sample_index / 2.
 *
 * === Pořadí „field“ na desce segmentu (STM musí plnit přesně takto) ===
 * Souřadnice v rámci čtvrtiny 4×4 (jen jeden plošňák):
 *   col = 0..3  — směr souborů od vnitřního rohu čtvrtiny k vnějšímu
 *                 (u segmentu u ESP: col 0 = a, col 3 = d).
 *   row = 0..3  — směr od hráče bílých k černým (row 0 = řada 1 u spodních
 *                 čtvrtin, row 3 = řada 4 u spodních čtvrtin).
 *   field = row * 4 + col
 *
 * Příklad segment 0 (DESKA s RESET u ESP, šachovnice a1–d4):
 *   field 0 = a1, 1 = b1, 2 = c1, 3 = d1,
 *   field 4 = a2, … , 15 = d4.
 *
 * Ostatní tři segmenty jsou stejně „row-major“ ve své lokální 4×4;
 * globální mapování a–h / 1–8 řeší firmware ESP (hall_map_segment_field_to_square).
 *
 * === Doporučení pro přesnost (STM) ===
 * - ADC 12 bitů je OK; drž konstantní Vref a čas integrace.
 * - Před zápisem do I2C bufferu uspořádej krátké průměrování (např. 8–32 vzorků
 *   na kanál) v jednom mux cyklu.
 * - Aktualizuj celých 64 B atomicky (stejný snapshot) — např. dvojbuffer:
 *   ADC naplňuje A, I2C čte B, pak prohoď.
 * - Hall páry u jednoho field mají být z stejného „ticku“ muxu, ne z dvou různých
 *   přepnutí, pokud to jde.
 *
 * === Výchozí I2C adresy (7 bitů, lze změnit straps / EEPROM na segmentu) ===
 * 0x30, 0x31, 0x32, 0x33 — odpovídá CONFIG_CHESS_HALL_SEGx_ADDR na ESP.
 */

static inline uint16_t hall_i2c_spec_read_le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void hall_i2c_spec_write_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
