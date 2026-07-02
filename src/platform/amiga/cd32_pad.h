/**
 * Amiga CD32 gamepad protocol (joystick port 2, Rev 5).
 *
 * Serial shift on DB-9 pins 5 (JOYMODE), 6 (CLOCK), 9 (DATA).
 * See doc/CD32_BUILD_SPEC.md and PSCD32 diary.
 */

#ifndef _PLATFORM_AMIGA_CD32_PAD_H
#define _PLATFORM_AMIGA_CD32_PAD_H

#include <stdbool.h>
#include <stdint.h>

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

void cd32_port2_init(void);
bool cd32_port2_is_enabled(void);
void cd32_port2_set_enabled(bool enabled);
void cd32_port2_toggle(void);

/** Update latched button state and D-pad (dir bits: up=1 down=2 left=4 right=8). */
void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits);

/** D-pad only refresh (when drivers call set_direction per axis). */
void cd32_port2_update_dpad(uint8_t direction_bits);

/** Map legacy 3-button port2 updates when CD32 mode is on (partial fallback). */
void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed);

#endif
