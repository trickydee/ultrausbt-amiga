/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Display interface for SSD1306 OLED
 */

#ifndef _DISPLAY_DISPLAY_H
#define _DISPLAY_DISPLAY_H

#include <stdint.h>

// Screen carousel pages (# advances; wraps to splash).
// Amiga host: Splash → Devices → Map Devices → Settings → Splash
// PC KBD / USB adapter: Splash → Settings → Splash (Devices/Map hidden)
typedef enum {
    DISPLAY_SCREEN_SPLASH = 0,
    DISPLAY_SCREEN_DEVICES = 1,
    DISPLAY_SCREEN_MAP_DEVICES = 2,
    DISPLAY_SCREEN_SETTINGS = 3,
} display_screen_t;

/**
 * Initialize the display
 * Must be called before any other display functions
 */
void display_init(void);

/**
 * Update the display with current device counts
 * Call this whenever device counts change
 */
void display_update_devices(void);

/**
 * Show the splash screen
 */
void display_show_splash(void);

/**
 * Show the devices screen (keyboards, mice, joysticks)
 */
void display_show_devices(void);

/**
 * Show the Map Devices screen (USB and Bluetooth device names per port)
 */
void display_show_map_devices(void);

/**
 * Show the Settings carousel page (Clear BT pair / USB Amiga↔PC KBD)
 */
void display_show_settings(void);

/**
 * Get current device counts (for external access)
 */
void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy);

/**
 * Set device counts (called from USB/Bluetooth handlers)
 */
void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy);
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy);

/**
 * Handle button presses (call from main loop)
 * Checks center button and advances the screen carousel
 */
void display_handle_buttons(void);

/**
 * Periodic display refresh work (call from main loop)
 */
void display_tick(void);

/**
 * Show controller detection message on OLED
 * @param controller_name Name of controller (e.g., "PS4", "PS3")
 * @param controller_model Model name (e.g., "DualShock 4")
 * @param duration_ms How long to show the message (milliseconds)
 */
void display_show_controller_detected(const char* controller_name, const char* controller_model, uint32_t duration_ms);

#endif // _DISPLAY_DISPLAY_H
