/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse keyboard path: read a real Amiga keyboard on KCLK/KDAT and forward
 * decoded keystrokes to the USB HID device layer (USB device mode only).
 */

#ifndef _PLATFORM_AMIGA_KEYBOARD_HOST_IN_H
#define _PLATFORM_AMIGA_KEYBOARD_HOST_IN_H

// Configure KCLK/KDAT as inputs, build the reverse keycode map and install the
// KCLK edge interrupt. Call once at boot when in USB device mode (instead of
// amiga_init(), which drives the lines toward the Amiga).
void keyboard_host_in_init(void);

// Drain captured keyboard frames: decode, translate to HID and hand to the USB
// device layer; also performs the keyboard handshake. Call from the main loop.
void keyboard_host_in_task(void);

#endif // _PLATFORM_AMIGA_KEYBOARD_HOST_IN_H
