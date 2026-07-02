/**
 * Amiga CD32 gamepad protocol (joystick ports 1 and 2, Rev 5).
 */

#ifndef _PLATFORM_AMIGA_CD32_PAD_H
#define _PLATFORM_AMIGA_CD32_PAD_H

#include <stdbool.h>
#include <stdint.h>

#include "joystick_port1.h"
#include "joystick_port2.h"

typedef struct {
    bool blue;
    bool red;
    bool yellow;
    bool green;
    bool ff;
    bool rew;
    bool pause;
} cd32_buttons_t;

void cd32_port1_init(void);
bool cd32_port1_is_enabled(void);
void cd32_port1_set_enabled(bool enabled);
void cd32_port1_toggle(void);
void cd32_port1_update(const cd32_buttons_t* buttons, uint8_t direction_bits);
void cd32_port1_update_dpad(uint8_t direction_bits);
void cd32_port1_legacy_button(enum amiga_joystick_port1_buttons button, bool pressed);

void cd32_port2_init(void);
bool cd32_port2_is_enabled(void);
void cd32_port2_set_enabled(bool enabled);
void cd32_port2_toggle(void);
void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits);
void cd32_port2_update_dpad(uint8_t direction_bits);
void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed);

#endif
