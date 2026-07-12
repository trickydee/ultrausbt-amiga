/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * USB role selection: normal HOST mode (read USB/BT devices, drive the Amiga) vs
 * DEVICE mode (read a real Amiga keyboard/mouse, present as a USB HID to a PC).
 *
 * The two roles share a single USB PHY, so exactly one is active. The mode is
 * persisted in flash and selected at boot. Toggling at runtime persists the new
 * mode and reboots into it (clean, deterministic USB enumeration on the host).
 */

#ifndef _USB_MODE_H
#define _USB_MODE_H

#include <stdbool.h>

typedef enum {
    USB_MODE_HOST   = 0,  // normal: USB/BT -> Amiga
    USB_MODE_DEVICE = 1,  // reverse: Amiga -> USB PC (HID keyboard/mouse)
} usb_mode_t;

// Load the persisted mode from flash. Call once early in main().
void usb_mode_init(void);

// Current mode (valid after usb_mode_init()).
usb_mode_t usb_mode_get(void);
bool usb_mode_is_device(void);

// Bring up the TinyUSB stack in the role matching the current mode. Call once from
// the core that services USB (Core 0), replacing the old tusb_init(host) call.
void usb_mode_start_usb(void);

// Persist the opposite mode and reboot into it. Safe to call from the main loop /
// button handler. Does not return (reboots).
void usb_mode_request_toggle(void);

#endif // _USB_MODE_H
