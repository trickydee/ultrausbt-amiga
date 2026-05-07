/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga joystick port 1 interface.
 * 
 * Joystick Port 1 shares the same GPIO pins as the mouse interface (GPIO 7-13).
 * This module provides joystick emulation using simple digital direction signals.
 */

#ifndef _PLATFORM_AMIGA_JOYSTICK_PORT1_H
#define _PLATFORM_AMIGA_JOYSTICK_PORT1_H

#include <stdint.h>
#include <stdbool.h>

enum amiga_joystick_port1_buttons {
    AJ1_FIRE,      // Button 1 (Fire)
    AJ1_BUTTON2,   // Button 2
    AJ1_BUTTON3    // Button 3
};

enum amiga_joystick_port1_direction {
    AJ1_UP,
    AJ1_DOWN,
    AJ1_LEFT,
    AJ1_RIGHT
};

void amiga_joystick_port1_init(void);
void amiga_joystick_port1_set_direction(enum amiga_joystick_port1_direction dir, bool active);
void amiga_joystick_port1_set_button(enum amiga_joystick_port1_buttons button, bool pressed);
void amiga_joystick_port1_set_from_mouse(int8_t x, int8_t y, uint8_t buttons);

// Port 1 mode toggle functions
// Port 1 can operate in two modes:
// - MOUSE mode: Only mouse quadrature signals (default)
// - JOYSTICK mode: Mouse input is converted to joystick signals
void amiga_joystick_port1_toggle_mode(void);
bool amiga_joystick_port1_is_joystick_mode(void);

#endif

