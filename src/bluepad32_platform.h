/**
 * bluepad32 platform header
 * public API for accessing Bluetooth keyboard data
 */

#ifndef _BLUEPAD32_PLATFORM_H
#define _BLUEPAD32_PLATFORM_H

#if ENABLE_BLUEPAD32

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration to avoid including uni.h (which causes HID type conflicts with TinyUSB)
// Get keyboard data for a specific index (0-1)
// Returns true if keyboard is connected and has data
// out_keyboard must point to a struct matching uni_keyboard_t layout
// Marks data as read (clears updated flag)
bool bluepad32_get_keyboard(int idx, void* out_keyboard);

// Peek at keyboard data without marking as read (for shortcuts)
// Returns true if keyboard is connected
// out_keyboard must point to a struct matching uni_keyboard_t layout
bool bluepad32_peek_keyboard(int idx, void* out_keyboard);

// Get count of connected Bluetooth keyboards
int bluepad32_get_keyboard_count(void);

// Get mouse data for a specific index (0-1)
// Returns true if mouse is connected and has data
// out_mouse must point to a struct matching uni_mouse_t layout
// Marks data as read (clears updated flag)
bool bluepad32_get_mouse(int idx, void* out_mouse);

// Get count of connected Bluetooth mice
int bluepad32_get_mouse_count(void);

// Get gamepad data for a specific index (0 for first gamepad)
// Returns true if gamepad is connected and has data
// out_gamepad must point to a struct matching uni_gamepad_t layout
// Marks data as read (clears updated flag)
bool bluepad32_get_gamepad(int idx, void* out_gamepad);

// Get count of connected Bluetooth gamepads
int bluepad32_get_gamepad_count(void);

// Delete all stored Bluetooth pairing keys
void bluepad32_delete_pairing_keys(void);

// Pairing mode runtime control
void bluepad32_pairing_start(void);
void bluepad32_pairing_stop(void);
bool bluepad32_pairing_is_active(void);

// Get Bluetooth device name for display
// Returns device name or NULL if not available
// idx: 0 = first device, 1 = second device
// device_type: 'J' for joystick/gamepad, 'K' for keyboard, 'M' for mouse
// Returns pointer to static string (do not free)
const char* bluepad32_get_device_name(char device_type, int idx);

// Platform function (needed by bluepad32_init.c)
struct uni_platform* get_my_platform(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_PLATFORM_H

