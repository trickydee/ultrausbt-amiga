/**
 * Unified joystick port 2 output (standard 3-button or CD32).
 */

#ifndef _PLATFORM_AMIGA_PORT2_GAMEPAD_H
#define _PLATFORM_AMIGA_PORT2_GAMEPAD_H

#include <stdbool.h>
#include <stdint.h>

bool port2_gamepad_cd32_mode(void);

/**
 * Submit port 2 gamepad state using Xbox-style face button names.
 * dir_bits: bit0=up, bit1=down, bit2=left, bit3=right
 */
void port2_gamepad_submit(uint8_t dir_bits,
                          bool face_a,
                          bool face_b,
                          bool face_x,
                          bool face_y,
                          bool l_shoulder,
                          bool r_shoulder,
                          bool start);

void port2_gamepad_clear(void);

#endif
