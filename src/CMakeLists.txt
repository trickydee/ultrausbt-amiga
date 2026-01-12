add_executable(amigahid-pico
  main.c
  usb_hid.c
)

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
