# Bluepad32 Bluetooth support (wireless boards only)
# Pico 2 W (pico2_w) has CYW43 for Bluetooth
if(PICO_BOARD STREQUAL "pico2_w" OR PICO_BOARD STREQUAL "pico_w")
    set(ENABLE_BLUEPAD32 ON)
    add_definitions(-DENABLE_BLUEPAD32=1)
    message(STATUS "Bluepad32: ENABLED (${PICO_BOARD} detected)")
else()
    set(ENABLE_BLUEPAD32 OFF)
    add_definitions(-DENABLE_BLUEPAD32=0)
    message(STATUS "Bluepad32: DISABLED (non-wireless board ${PICO_BOARD})")
endif()

add_executable(amigahid-pico
  main.c
  usb_hid.c
)

# Add Bluepad32 source files if enabled
if(ENABLE_BLUEPAD32)
    target_sources(amigahid-pico PRIVATE
        bluepad32_init.c
        bluepad32_platform.c
    )
endif()

add_subdirectory(platform)
add_subdirectory(util)
add_subdirectory(display)

# make the src subdirectory availabe in the include path so tinyusb can find tusb_config.h
target_include_directories(amigahid-pico PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_uart(amigahid-pico 1)

target_link_libraries(amigahid-pico PUBLIC
  pico_stdlib
  pico_multicore
  hardware_dma
  hardware_gpio
  hardware_i2c
  hardware_pio
  tinyusb_host
  tinyusb_board
)

# Add Bluepad32 libraries if enabled
if(ENABLE_BLUEPAD32)
    # Add bluepad32 subdirectory first
    add_subdirectory(${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32 libbluepad32)
    
    # Add include directory for btstack_config.h to bluepad32 library
    target_include_directories(bluepad32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    
    # Add Bluepad32 include directories
    target_include_directories(amigahid-pico PRIVATE
        ${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32/include
        ${CMAKE_CURRENT_SOURCE_DIR}  # For btstack_config.h and sdkconfig.h
        ${PICO_SDK_PATH}/lib/btstack/src  # For btstack headers
    )
    
    target_link_libraries(amigahid-pico PUBLIC
        pico_cyw43_arch_none
        pico_btstack_ble      # Required for GATT client functions (HID service discovery)
        pico_btstack_classic
        pico_btstack_cyw43
        pico_async_context_poll  # For async_context_poll_init_with_defaults
        bluepad32
    )
endif()

add_compile_definitions(PICO_USE_MALLOC_MUTEX=0)

# Set output directory to build/ instead of build/src/
# CMAKE_CURRENT_BINARY_DIR is build/src when we're in this subdirectory, so we need to go up one level
# Compute the parent binary directory (build/) by going up from CMAKE_CURRENT_BINARY_DIR
get_filename_component(parent_binary_dir "${CMAKE_CURRENT_BINARY_DIR}/.." ABSOLUTE)
set_target_properties(amigahid-pico PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${parent_binary_dir}"
    ARCHIVE_OUTPUT_DIRECTORY "${parent_binary_dir}"
)

pico_add_extra_outputs(amigahid-pico)
