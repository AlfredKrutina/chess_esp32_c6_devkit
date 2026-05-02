# STM32C031 — Hall I2C slave + 2× CD74HC4067 (KiCad matrix): I2C1 PB6/PB7 (TSSOP20).
# Flash: F4/x4 = 16K → make FLASH_KB=16 ; F6/C6 = 32K (výchozí).
# Vyžaduje: arm-none-eabi-gcc, inicializované submoduly v ../STM32CubeC0.

CUBE := ../STM32CubeC0
TPL := $(CUBE)/Projects/STM32C0316-DK/Templates_LL

PREFIX := arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size

BUILD_DIR := build
ELF := $(BUILD_DIR)/stm32_hall_c031.elf
BIN := $(BUILD_DIR)/stm32_hall_c031.bin

FLASH_KB ?= 32
LDSCRIPT := $(if $(filter 16,$(FLASH_KB)),STM32C031_FLASH_16K.ld,STM32C031C6TX_FLASH.ld)

HALL_I2C_OWNADDR7BIT ?= 0x30

STUB_INC := $(abspath toolchain_include)

CPU := -mcpu=cortex-m0plus -mthumb
# Brew arm-none-eabi-gcc často nemá newlib sysroot — vlastní minimální hlavičky + bez systémových include.
INCLUDES := -nostdinc \
	-isystem $(STUB_INC) \
	-IInc \
	-isystem $(CUBE)/Drivers/CMSIS/Include \
	-isystem $(CUBE)/Drivers/CMSIS/Device/ST/STM32C0xx/Include \
	-isystem $(CUBE)/Drivers/STM32C0xx_HAL_Driver/Inc

DEFS := -DSTM32C031xx -DUSE_FULL_LL_DRIVER \
	-DHALL_I2C_OWNADDR7BIT=$(HALL_I2C_OWNADDR7BIT)

CFLAGS := $(CPU) $(DEFS) $(INCLUDES) -Og -g \
	-std=c11 -Wall -Wextra -Wno-unused-parameter \
	-ffunction-sections -fdata-sections

ASMFLAGS := $(CPU) $(DEFS) $(INCLUDES)

# Brew toolchain bez nano.specs — jen libgcc (+ vlastní syscalls/memset).
LDFLAGS := $(CPU) -T$(LDSCRIPT) -nostartfiles \
	-Wl,-Map=$(BUILD_DIR)/stm32_hall_c031.map -Wl,--gc-sections \
	-nostdlib

SRC_SYS := $(TPL)/Src/system_stm32c0xx.c
SRC_CUBE_LL := $(CUBE)/Drivers/STM32C0xx_HAL_Driver/Src
SRC_USER := $(TPL)/STM32CubeIDE/Application/User
STARTUP := $(TPL)/STM32CubeIDE/Application/Startup/startup_stm32c031c6tx.s

OBJS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/stm32c0xx_it.o \
	$(BUILD_DIR)/crt_stub.o \
	$(BUILD_DIR)/libc_stubs.o \
	$(BUILD_DIR)/memset.o \
	$(BUILD_DIR)/system_stm32c0xx.o \
	$(BUILD_DIR)/stm32c0xx_ll_gpio.o \
	$(BUILD_DIR)/stm32c0xx_ll_rcc.o \
	$(BUILD_DIR)/stm32c0xx_ll_utils.o \
	$(BUILD_DIR)/stm32c0xx_ll_i2c.o \
	$(BUILD_DIR)/stm32c0xx_ll_pwr.o \
	$(BUILD_DIR)/stm32c0xx_ll_adc.o \
	$(BUILD_DIR)/syscalls.o \
	$(BUILD_DIR)/sysmem_local.o \
	$(BUILD_DIR)/startup_stm32c031c6tx.o

.PHONY: all clean bin size copy-embedded

all: $(ELF) bin size

bin: $(BIN)

# Volitelně překopírovat firmware pro ESP oddíl stm32_fw (kořen repozitáře ../../embedded/).
copy-embedded: $(BIN)
	cp $(BIN) ../../embedded/stm32_fw_embedded.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ELF): $(OBJS) $(LDSCRIPT) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -lgcc -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

size: $(ELF)
	$(SIZE) $<

$(BUILD_DIR)/main.o: Src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_it.o: Src/stm32c0xx_it.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/crt_stub.o: Src/crt_stub.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc_stubs.o: Src/libc_stubs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memset.o: Src/memset.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32c0xx.o: $(SRC_SYS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_gpio.o: $(SRC_CUBE_LL)/stm32c0xx_ll_gpio.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_rcc.o: $(SRC_CUBE_LL)/stm32c0xx_ll_rcc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_utils.o: $(SRC_CUBE_LL)/stm32c0xx_ll_utils.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_i2c.o: $(SRC_CUBE_LL)/stm32c0xx_ll_i2c.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_pwr.o: $(SRC_CUBE_LL)/stm32c0xx_ll_pwr.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32c0xx_ll_adc.o: $(SRC_CUBE_LL)/stm32c0xx_ll_adc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscalls.o: $(SRC_USER)/syscalls.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sysmem_local.o: Src/sysmem.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup_stm32c031c6tx.o: $(STARTUP) | $(BUILD_DIR)
	$(CC) $(ASMFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
