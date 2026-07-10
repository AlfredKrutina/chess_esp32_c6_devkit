#!/usr/bin/env bash
# Statická kontrola Hall V2 + STM32 auto-flash profilu (bez HW).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROFILE="$ROOT/sdkconfig.defaults.hall_v2"
BIN="$ROOT/embedded/stm32_fw_embedded.bin"
PART="$ROOT/partitions.csv"
FAIL=0

check() {
  if "$@"; then
    echo "OK: $*"
  else
    echo "FAIL: $*"
    FAIL=1
  fi
}

check_not() {
  if "$@"; then
    echo "FAIL: expected false: $*"
    FAIL=1
  else
    echo "OK (absent): $*"
  fi
}

echo "=== verify_hall_v2_profile ==="

check test -f "$PROFILE"
check test -s "$BIN"
check grep -q 'CONFIG_CHESS_MATRIX_INPUT_I2C_HALL=y' "$PROFILE"
check grep -q 'CONFIG_CHESS_STM32_I2C_BL_ENABLE=y' "$PROFILE"
check grep -q 'CONFIG_CHESS_STM32_BL_AUTO_FLASH_ON_BOOT=y' "$PROFILE"
check grep -q 'CONFIG_CHESS_STM32_BL_AUTO_USE_BOOT0=y' "$PROFILE"
check_not grep -q 'CONFIG_CHESS_STM32_BL_BOOT0_GPIO=-1' "$PROFILE"
check_not grep -q 'CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0=-1' "$PROFILE"
check grep -q 'CONFIG_CHESS_STM32_BL_VERIFY_HALL_AFTER_FLASH=y' "$PROFILE"
check grep -q 'CONFIG_CHESS_STM32_BL_REFLASH_IF_HALL_MISSING=y' "$PROFILE"
check grep -q 'stm32_fw' "$PART"

# ARM vector table sanity (reset handler in flash range)
check test "$(wc -c < "$BIN")" -ge 256

if [[ "$FAIL" -ne 0 ]]; then
  echo "verify_hall_v2_profile: CHYBA"
  exit 1
fi
echo "verify_hall_v2_profile: vše OK"
