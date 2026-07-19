/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Unified port 1 gamepad routing (standard 3-button or CD32).
 */

#include "port1_gamepad.h"
#include "cd32_pad.h"
#include "joystick_port1.h"

bool port1_gamepad_cd32_mode(void) {
    return cd32_port1_is_enabled();
}

void port1_gamepad_submit(uint8_t dir_bits,
                          bool face_a,
                          bool face_b,
                          bool face_x,
                          bool face_y,
                          bool l_shoulder,
                          bool r_shoulder,
                          bool start) {
    if (cd32_port1_is_enabled()) {
        cd32_buttons_t buttons = {
            .blue = face_b,
            .red = face_a,
            .yellow = face_y,
            .green = face_x,
            .ff = r_shoulder,
            .rew = l_shoulder,
            .pause = start,
        };
        cd32_port1_update(&buttons, dir_bits);
        return;
    }

    amiga_joystick_port1_set_direction(AJ1_UP, (dir_bits & 0x01) != 0);
    amiga_joystick_port1_set_direction(AJ1_DOWN, (dir_bits & 0x02) != 0);
    amiga_joystick_port1_set_direction(AJ1_LEFT, (dir_bits & 0x04) != 0);
    amiga_joystick_port1_set_direction(AJ1_RIGHT, (dir_bits & 0x08) != 0);
    amiga_joystick_port1_set_button(AJ1_FIRE, face_a);
    amiga_joystick_port1_set_button(AJ1_BUTTON2, face_b);
    amiga_joystick_port1_set_button(AJ1_BUTTON3, face_x || face_y);
}

void port1_gamepad_clear(void) {
    if (cd32_port1_is_enabled()) {
        cd32_buttons_t buttons = {0};
        cd32_port1_update(&buttons, 0);
        return;
    }
    amiga_joystick_port1_set_direction(AJ1_UP, false);
    amiga_joystick_port1_set_direction(AJ1_DOWN, false);
    amiga_joystick_port1_set_direction(AJ1_LEFT, false);
    amiga_joystick_port1_set_direction(AJ1_RIGHT, false);
    amiga_joystick_port1_set_button(AJ1_FIRE, false);
    amiga_joystick_port1_set_button(AJ1_BUTTON2, false);
    amiga_joystick_port1_set_button(AJ1_BUTTON3, false);
}
