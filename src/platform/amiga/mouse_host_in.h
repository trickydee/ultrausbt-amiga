/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse mouse path: read an Amiga quadrature mouse on Port 1 and forward
 * relative motion + buttons to the USB HID device layer (USB device mode only).
 */

#ifndef _PLATFORM_AMIGA_MOUSE_HOST_IN_H
#define _PLATFORM_AMIGA_MOUSE_HOST_IN_H

// Configure Port 1 quadrature + button pins as inputs. Call once at boot in USB
// device mode (Core 1 quadrature *output* must not be running).
void mouse_host_in_init(void);

// Poll the quadrature lines and, at a fixed cadence, emit a USB mouse report.
// Call frequently from the main loop.
void mouse_host_in_task(void);

#endif // _PLATFORM_AMIGA_MOUSE_HOST_IN_H
