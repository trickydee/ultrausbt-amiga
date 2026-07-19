/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * https://github.com/borb/amigahid-pico
 *
 * Modifications Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga joystick port 2 interface.
 *
 * Joystick Port 2 uses dedicated GPIO pins (GPIO 18-22, 26-27 for Revision 5).
 * This module provides joystick emulation using simple digital direction signals.
 */

#ifndef _PLATFORM_AMIGA_JOYSTICK_PORT2_H
#define _PLATFORM_AMIGA_JOYSTICK_PORT2_H

#include <stdint.h>
#include <stdbool.h>

enum amiga_joystick_port2_buttons {
    AJ2_FIRE,      // Button 1 (Fire)
    AJ2_BUTTON2,   // Button 2
    AJ2_BUTTON3    // Button 3
};

enum amiga_joystick_port2_direction {
    AJ2_UP,
    AJ2_DOWN,
    AJ2_LEFT,
    AJ2_RIGHT
};

void amiga_joystick_port2_init(void);
void amiga_joystick_port2_set_direction(enum amiga_joystick_port2_direction dir, bool active);
void amiga_joystick_port2_set_button(enum amiga_joystick_port2_buttons button, bool pressed);
void amiga_joystick_port2_reset(void);

// Getter functions for debugging
bool amiga_joystick_port2_get_direction(enum amiga_joystick_port2_direction dir);
bool amiga_joystick_port2_get_button(enum amiga_joystick_port2_buttons button);

#endif
