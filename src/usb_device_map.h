/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * USB device names for the Map Devices OLED screen.
 */

#ifndef _USB_DEVICE_MAP_H
#define _USB_DEVICE_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define USB_MAP_NAME_LEN 24

void usb_map_register_gamepad(uint8_t dev_addr, const char* name);
void usb_map_unregister_gamepad(uint8_t dev_addr);
bool usb_map_gamepad_registered(uint8_t dev_addr);
const char* usb_map_get_gamepad(int slot);

void usb_map_set_keyboard(const char* name);
void usb_map_clear_keyboard(void);
const char* usb_map_get_keyboard(void);

void usb_map_set_mouse(const char* name);
void usb_map_clear_mouse(void);
const char* usb_map_get_mouse(void);

#endif // _USB_DEVICE_MAP_H
