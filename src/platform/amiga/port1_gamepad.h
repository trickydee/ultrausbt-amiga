/**
 * Unified joystick port 1 output (standard 3-button or CD32).
 */

#ifndef _PLATFORM_AMIGA_PORT1_GAMEPAD_H
#define _PLATFORM_AMIGA_PORT1_GAMEPAD_H

#include <stdbool.h>
#include <stdint.h>

bool port1_gamepad_cd32_mode(void);

void port1_gamepad_submit(uint8_t dir_bits,
                          bool face_a,
                          bool face_b,
                          bool face_x,
                          bool face_y,
                          bool l_shoulder,
                          bool r_shoulder,
                          bool start);

void port1_gamepad_clear(void);

#endif
