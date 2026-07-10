#!/usr/bin/env bash
# Sestaví demo STM32 Hall firmware a zkopíruje do embedded/stm32_fw_embedded.bin
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/firmware/stm32_hall_c031"
make demo-embedded
echo "OK: $ROOT/embedded/stm32_fw_embedded.bin"
