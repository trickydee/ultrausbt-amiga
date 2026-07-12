cmake_minimum_required(VERSION 3.13)

set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/pico-sdk/")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(PICO_TINYUSB_PATH "${CMAKE_SOURCE_DIR}/tinyusb/")

include(cmake/pico_sdk_import.cmake)

add_compile_options(-Wall)
# Note: -Werror removed to allow btstack (pico-sdk) to compile with warnings
# TODO: Re-enable -Werror once btstack warnings are resolved

# configuration options

# Uncomment for verbose UART debug (ahprintf, Core1 DIAG, BT pause DIAG, etc.)
# add_compile_definitions(DEBUG_MESSAGES=1)

# Board revision (pin map in src/config.h):
#   5 = current shipping PCB (level shifters; Port 2 fire/B2/B3 on GPIO 26/27/28)
#   6 = direct-5V Pico 2 PCB (Port 2 fire/B2/B3 on GPIO 16/17/18; OLED buttons on ADC 26/27/28;
#       GPIO 0/1 = debug UART) — see doc/gpio_rev6_adc_avoidance.md
add_compile_definitions(HIDPICO_REVISION=5)

# Overclock Pico to 200 MHz (all boards, requires Pico SDK 2.1.1+)
# The SDK will automatically adjust the regulator voltage to support this clock speed
# This will be applied conditionally in src/CMakeLists.txt based on board type

# debugging for tinyusb - be warned that it can cause timing issues causing things to break
# add_compile_definitions(CFG_TUSB_DEBUG=2)

project(amigahid-pico C CXX ASM)

pico_sdk_init()

add_subdirectory(src)
