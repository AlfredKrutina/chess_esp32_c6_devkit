# ESP32-C6 CZECHMATE — CMakeLists.txt
# Complete chess system with WS2812B + 8x8 Matrix + Button LED Feedback
# Author: Alfred Krutina
# Verze firmware (esp_app_desc / API): baseline pro OTA testy z mobilu — zvyšuj manifest v aplikaci.
# Binární název projektu zůstává esp32_chess_v24 kvůli CI/partition zvyklostem.

cmake_minimum_required(VERSION 3.16)

# Project configuration
set(PROJECT_NAME "esp32_chess_v24")
set(PROJECT_VERSION "1.1.1")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set project name and version
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION})

# Oddíl stm32_fw: při `idf.py flash` zapíše STM32 .bin (ESP pak může STM přes I2C ROM BL nabootstrakovat).
if(EXISTS "${CMAKE_SOURCE_DIR}/embedded/stm32_fw_embedded.bin")
  esptool_py_flash_to_partition(flash stm32_fw "${CMAKE_SOURCE_DIR}/embedded/stm32_fw_embedded.bin")
endif()

# CHIP/Matter C++: benign -Wtype-limits on EndpointId vs FIXED_ENDPOINT_COUNT (third-party connectedhomeip).
idf_build_set_property(CXX_COMPILE_OPTIONS "-Wno-type-limits" APPEND)

# Component configuration
set(EXTRA_COMPONENT_DIRS 
    components/freertos_chess
    components/led_task
    components/matrix_task  
    components/button_task
    components/uart_task
    components/game_task
    components/animation_task
    components/screen_saver_task
    components/test_task
    components/matter_task
    components/web_server_task
    components/config_manager
    components/promotion_button_task
    components/reset_button_task
    components/game_led_animations
    components/uart_commands_extended
    components/unified_animation_manager
    components/led_state_manager
    components/visual_error_system
    components/game_hooks
    components/ble_task
    components/stm32_i2c_bootloader
)

# Project-wide compile definitions
add_compile_definitions(
    CHESS_VERSION_MAJOR=1
    CHESS_VERSION_MINOR=1
    CHESS_VERSION_PATCH=1
    CHESS_VERSION_STRING="v1.1.1"
    CHESS_BUILD_DATE="2026-05-05"
)

# Compiler flags for optimization and debugging
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

# Debug/Release specific flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(CHESS_DEBUG_MODE=1)
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
else()
    add_compile_definitions(CHESS_DEBUG_MODE=0)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Print configuration summary
message(STATUS "========================================")
message(STATUS "ESP32-C6 Chess System v1.1.1 Configuration")
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Target: ESP32-C6")
message(STATUS "Features:")
message(STATUS "  ✓ WS2812B 73 LEDs (64 board + 9 buttons)")
message(STATUS "  ✓ 8x8 Reed Switch Matrix")
message(STATUS "  ✓ Button LED Feedback")
message(STATUS "  ✓ Time-multiplexing GPIO")
message(STATUS "  ✓ UART Command Interface")
message(STATUS "  ✓ Real Hardware Support")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "IDF Version: $ENV{IDF_VERSION}")
message(STATUS "IDF Path: $ENV{IDF_PATH}")
message(STATUS "========================================")

# Custom build targets
add_custom_target(info
    COMMAND ${CMAKE_COMMAND} -E echo "ESP32-C6 Chess v1.1.1 Build Information:"
    COMMAND ${CMAKE_COMMAND} -E echo "  Project: ${PROJECT_NAME}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Version: ${PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Build Type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "  Target: ESP32-C6"
    COMMAND ${CMAKE_COMMAND} -E echo "  Components: ${EXTRA_COMPONENT_DIRS}"
)

# Memory analysis target
add_custom_target(memory
    COMMAND ${CMAKE_COMMAND} -E echo "Analyzing memory usage..."
    COMMAND python $ENV{IDF_PATH}/tools/esp_app_trace/sysviewtrace_proc.py size ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf
    DEPENDS ${PROJECT_NAME}.elf
)
