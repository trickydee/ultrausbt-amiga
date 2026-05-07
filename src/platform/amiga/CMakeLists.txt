pico_generate_pio_header(amigahid-pico ${CMAKE_CURRENT_LIST_DIR}/keyboard.pio)

target_sources(amigahid-pico PRIVATE keyboard_serial_io.c quad_mouse.c joystick_port1.c joystick_port2.c mouse_config.c)
