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
 * keyboard + mouse. Used by "USB device mode" where a real Amiga keyboard/mouse
 * is read and forwarded to the PC. Modelled on the ultramegausb Apple-ADB build.
 */

#ifndef _USB_HID_DEVICE_H
#define _USB_HID_DEVICE_H

#include <stdint.h>

// Latch a keyboard report (6-key rollover + modifier byte). The actual USB report
// is sent from usb_hid_device_task() when the endpoint is ready.
void usb_hid_device_send_keyboard(uint8_t modifier, const uint8_t keycodes[6]);

// Latch a relative mouse report.
void usb_hid_device_send_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);

// Synthesize a caps-lock press+release pulse (Amiga caps is a locking key; this
// keeps the host's caps state in sync with each physical Amiga caps toggle).
void usb_hid_device_pulse_caps_lock(void);

// Drain latched reports to the host. Call frequently from the USB-servicing loop
// while in device mode (alongside tud_task()).
void usb_hid_device_task(void);

// Most recent host LED state (bit0 numlock, bit1 capslock, ...). 0 if not mounted.
uint8_t usb_hid_device_led_state(void);

#endif // _USB_HID_DEVICE_H
