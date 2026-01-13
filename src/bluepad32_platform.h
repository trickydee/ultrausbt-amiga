/**
 * this file is part of amigahid-pico, (c) 2024
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
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

// Delete all stored Bluetooth pairing keys
void bluepad32_delete_pairing_keys(void);

// Platform function (needed by bluepad32_init.c)
struct uni_platform* get_my_platform(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_PLATFORM_H

