/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#ifndef _PLATFORM_AMIGA_QUAD_MOUSE_H
#define _PLATFORM_AMIGA_QUAD_MOUSE_H

#include <stdint.h>
#include <stdbool.h>

enum amiga_quad_mouse_buttons { AQM_LEFT, AQM_MIDDLE, AQM_RIGHT };

// Mouse type: Amiga (default) or Atari (swapped pins 1 and 4)
typedef enum {
    MOUSE_TYPE_AMIGA = 0,
    MOUSE_TYPE_ATARI = 1
} mouse_type_t;

void amiga_quad_mouse_init();
void amiga_quad_mouse_motion();
void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed);
void amiga_quad_mouse_set_motion(int8_t in_x, int8_t in_y);

// Core 1 loop counter (Core 0 reads for stall detection / DIAG)
extern volatile uint32_t g_core1_heartbeat;

// Mouse type functions
void amiga_quad_mouse_set_type(mouse_type_t type);
mouse_type_t amiga_quad_mouse_get_type(void);
void amiga_quad_mouse_toggle_type(void);

// Port 1 CD32 mode — pauses quadrature GPIO on Core 1 (independent of BT pause depth)
void amiga_quad_mouse_pause_core1(void);
void amiga_quad_mouse_resume_core1(void);

// Bluetooth gamepad enumeration — refcounted pause (Atari v22.1.0)
void core1_pause_for_bt_enumeration(void);
void core1_resume_after_bt_enumeration(void);
void core1_wait_for_pause_active(uint32_t timeout_ms);
uint32_t core1_get_bt_pause_depth(void);
void core1_force_release_bt_pause(void);
// Returns true if watchdog forced a release
bool core1_bt_pause_watchdog_tick(void);

/** Core 0: detect stalled Core 1 heartbeat and force-wake / relaunch motion loop. */
bool core1_heartbeat_watchdog_tick(void);

#endif
