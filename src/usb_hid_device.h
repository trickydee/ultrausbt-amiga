/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * USB HID *device* stack: presents the adapter to a host PC as a composite
 * keyboard + mouse + gamepad. Used by "USB device mode" / Host Mode UI where a
 * real Amiga keyboard/mouse and Port 2 Atari stick are forwarded to the PC.
 */

#ifndef _USB_HID_DEVICE_H
#define _USB_HID_DEVICE_H

#include <stdint.h>

void usb_hid_device_send_keyboard(uint8_t modifier, const uint8_t keycodes[6]);
void usb_hid_device_send_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);
// x/y: digital d-pad as left-stick axes (-127..127). hat: GAMEPAD_HAT_*.
void usb_hid_device_send_gamepad(int8_t x, int8_t y, uint8_t hat, uint32_t buttons);
void usb_hid_device_pulse_caps_lock(void);
void usb_hid_device_task(void);
uint8_t usb_hid_device_led_state(void);

#endif // _USB_HID_DEVICE_H
