/**
 * Unified port 2 gamepad routing (standard joystick vs CD32).
 */

#include "port2_gamepad.h"
#include "cd32_pad.h"
#include "joystick_port2.h"

bool port2_gamepad_cd32_mode(void) {
    return cd32_port2_is_enabled();
}

void port2_gamepad_submit(uint8_t dir_bits,
                          bool face_a,
                          bool face_b,
                          bool face_x,
                          bool face_y,
                          bool l_shoulder,
                          bool r_shoulder,
                          bool start) {
    if (cd32_port2_is_enabled()) {
        cd32_buttons_t buttons = {
            .blue = face_b,
            .red = face_a,
            .yellow = face_y,
            .green = face_x,
            .ff = r_shoulder,
            .rew = l_shoulder,
            .pause = start,
        };
        cd32_port2_update(&buttons, dir_bits);
        return;
    }

    amiga_joystick_port2_set_direction(AJ2_UP, (dir_bits & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN, (dir_bits & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT, (dir_bits & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, (dir_bits & 0x08) != 0);
    amiga_joystick_port2_set_button(AJ2_FIRE, face_a);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, face_b);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, face_x || face_y);
}

void port2_gamepad_clear(void) {
    if (cd32_port2_is_enabled()) {
        cd32_buttons_t buttons = {0};
        cd32_port2_update(&buttons, 0);
        return;
    }
    amiga_joystick_port2_reset();
}
