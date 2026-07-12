/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
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
