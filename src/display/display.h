/**
 * Display interface for SSD1306 OLED
 */

#ifndef _DISPLAY_DISPLAY_H
#define _DISPLAY_DISPLAY_H

#include <stdint.h>

// Display screens
typedef enum {
    DISPLAY_SCREEN_SPLASH = 0,
    DISPLAY_SCREEN_DEVICES = 1,
    DISPLAY_SCREEN_BT_NAMES = 2,
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
 * Show the Bluetooth device names screen
 */
void display_show_bt_names(void);

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
 * Checks center button and toggles between splash and devices screens
 */
void display_handle_buttons(void);

#endif // _DISPLAY_DISPLAY_H

