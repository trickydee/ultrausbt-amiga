/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse Port 2 path: read an Atari-style digital joystick (dirs + fire +
 * button 2) and forward it as a USB HID gamepad (USB device / Host Mode only).
 */

#ifndef _PLATFORM_AMIGA_JOYSTICK_PORT2_HOST_IN_H
#define _PLATFORM_AMIGA_JOYSTICK_PORT2_HOST_IN_H

void joystick_port2_host_in_init(void);
void joystick_port2_host_in_task(void);

#endif // _PLATFORM_AMIGA_JOYSTICK_PORT2_HOST_IN_H
