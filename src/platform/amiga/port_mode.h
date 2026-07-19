/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Port 1 / Port 2 mode management with flash persistence.
 */

#ifndef _PLATFORM_AMIGA_PORT_MODE_H
#define _PLATFORM_AMIGA_PORT_MODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PORT1_MODE_MOUSE = 0,
    PORT1_MODE_JOY,
    PORT1_MODE_LLAMA,
    PORT1_MODE_CD32,
} port1_mode_t;

void port_mode_init(void);

port1_mode_t port_mode_get_port1(void);
bool port_mode_get_port2_cd32(void);

/** Short label for OLED (MOUSE, JOY, LTRON, CD32). */
const char* port_mode_port1_label(void);

/** OLED / splash left button: MOUSE -> JOY -> LLAMA -> CD32 -> MOUSE */
void port_mode_cycle_port1(void);

/** Keyboard chord: toggle MOUSE <-> JOY only (no-op in LLAMA/CD32). */
void port_mode_toggle_port1_mouse_joy(void);

/** Keyboard chord: toggle LLAMA on/off (from JOY or MOUSE). */
void port_mode_toggle_port1_llamatron(void);

void port_mode_toggle_port2_cd32(void);
void port_mode_set_port2_cd32(bool enabled);

#endif
