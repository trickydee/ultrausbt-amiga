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
    PORT1_MODE_MOUSE = 0,       /* Amiga mouse (DB-9 quadrature) */
    PORT1_MODE_JOY,
    PORT1_MODE_LLAMA,
    PORT1_MODE_CD32,
    PORT1_MODE_MOUSE_ATARI,     /* Atari ST mouse pinout on Port 1 */
} port1_mode_t;

void port_mode_init(void);

port1_mode_t port_mode_get_port1(void);
bool port_mode_get_port2_cd32(void);

/** Short OLED labels (Ami Ms, Joy, CD32, Llama, Atr Ms). */
const char* port_mode_port1_label(void);
/** Port 2 OLED label: Joy or CD32. */
const char* port_mode_port2_label(void);

/** OLED left: AmiMs → Joy → CD32 → Llama → AtrMs → AmiMs */
void port_mode_cycle_port1(void);

/** OLED right: Joy ↔ CD32 */
void port_mode_cycle_port2(void);

/** Keyboard chord: toggle mouse (Ami) ↔ JOY only (no-op in LLAMA/CD32/Atr). */
void port_mode_toggle_port1_mouse_joy(void);

/** Keyboard chord: toggle LLAMA on/off (from JOY or either mouse). */
void port_mode_toggle_port1_llamatron(void);

void port_mode_toggle_port2_cd32(void);
void port_mode_set_port2_cd32(bool enabled);

#endif
