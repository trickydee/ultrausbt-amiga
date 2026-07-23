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

#include "port_mode.h"
#include "mouse_config.h"
#include "cd32_pad.h"
#include "quad_mouse.h"
#include "joystick_port1.h"
#include "config.h"
#include <stdio.h>

#if HIDPICO_REV_ATARI_BOARD

extern bool usb_hid_get_llamatron_mode(void);
extern void usb_hid_set_llamatron_mode(bool enabled);

static port1_mode_t g_port1_mode = PORT1_MODE_MOUSE;
static bool g_port2_cd32;

static bool port1_is_mouse_mode(port1_mode_t mode) {
    return mode == PORT1_MODE_MOUSE || mode == PORT1_MODE_MOUSE_ATARI;
}

static void port_mode_persist(void) {
    port_config_data_t config;
    config.mouse_type = amiga_quad_mouse_get_type();
    config.port1_mode = g_port1_mode;
    config.port2_cd32 = g_port2_cd32;
    port_config_save(&config);
}

static void port_mode_apply(void) {
    bool want_joy = !port1_is_mouse_mode(g_port1_mode);
    bool want_llama = (g_port1_mode == PORT1_MODE_LLAMA);
    bool want_port1_cd32 = (g_port1_mode == PORT1_MODE_CD32);
    bool want_port2_cd32 = g_port2_cd32;

    if (want_port1_cd32) {
        /* Port 1 CD32 and Llamatron both own Port 1 — exclusive. */
        want_llama = false;
    }
    /* Port 2 CD32 is independent: twin-stick can still drive Port 2 (CD32-aware). */

    /* Sync Amiga vs Atari mouse pinout from Port 1 mode. */
    if (g_port1_mode == PORT1_MODE_MOUSE_ATARI) {
        if (amiga_quad_mouse_get_type() != MOUSE_TYPE_ATARI) {
            amiga_quad_mouse_set_type(MOUSE_TYPE_ATARI);
        }
    } else if (g_port1_mode == PORT1_MODE_MOUSE) {
        if (amiga_quad_mouse_get_type() != MOUSE_TYPE_AMIGA) {
            amiga_quad_mouse_set_type(MOUSE_TYPE_AMIGA);
        }
    }

    cd32_port2_set_enabled(want_port2_cd32);
    cd32_port1_set_enabled(want_port1_cd32);

    if (want_port1_cd32) {
        amiga_quad_mouse_pause_core1();
    } else {
        amiga_quad_mouse_resume_core1();
    }

    if (amiga_joystick_port1_is_joystick_mode() != want_joy) {
        amiga_joystick_port1_toggle_mode();
    }

    if (usb_hid_get_llamatron_mode() != want_llama) {
        usb_hid_set_llamatron_mode(want_llama);
    }
}

void port_mode_init(void) {
    port_config_data_t config;
    port_config_load(&config);
    g_port1_mode = config.port1_mode;
    g_port2_cd32 = config.port2_cd32;

    /* Migrate older saves: Atari mouse type + generic MOUSE mode → MOUSE_ATARI. */
    if (g_port1_mode == PORT1_MODE_MOUSE && config.mouse_type == MOUSE_TYPE_ATARI) {
        g_port1_mode = PORT1_MODE_MOUSE_ATARI;
    }

    port_mode_apply();
}

port1_mode_t port_mode_get_port1(void) {
    return g_port1_mode;
}

bool port_mode_get_port2_cd32(void) {
    return g_port2_cd32;
}

const char* port_mode_port1_label(void) {
    switch (g_port1_mode) {
        case PORT1_MODE_JOY:         return "Joy";
        case PORT1_MODE_LLAMA:       return "Llama";
        case PORT1_MODE_CD32:        return "CD32";
        case PORT1_MODE_MOUSE_ATARI: return "Atr Ms";
        case PORT1_MODE_MOUSE:
        default:                     return "Ami Ms";
    }
}

const char* port_mode_port2_label(void) {
    return g_port2_cd32 ? "CD32" : "Joy";
}

void port_mode_cycle_port1(void) {
    /* AmiMs → Joy → CD32 → Llama → AtrMs → AmiMs */
    switch (g_port1_mode) {
        case PORT1_MODE_MOUSE:       g_port1_mode = PORT1_MODE_JOY; break;
        case PORT1_MODE_JOY:         g_port1_mode = PORT1_MODE_CD32; break;
        case PORT1_MODE_CD32:        g_port1_mode = PORT1_MODE_LLAMA; break;
        case PORT1_MODE_LLAMA:       g_port1_mode = PORT1_MODE_MOUSE_ATARI; break;
        case PORT1_MODE_MOUSE_ATARI:
        default:                     g_port1_mode = PORT1_MODE_MOUSE; break;
    }
    port_mode_apply();
    port_mode_persist();
    printf("[PORT] Port 1 mode: %s\n", port_mode_port1_label());
}

void port_mode_cycle_port2(void) {
    port_mode_toggle_port2_cd32();
}

void port_mode_toggle_port1_mouse_joy(void) {
    if (port1_is_mouse_mode(g_port1_mode)) {
        g_port1_mode = PORT1_MODE_JOY;
    } else if (g_port1_mode == PORT1_MODE_JOY) {
        g_port1_mode = PORT1_MODE_MOUSE;
    } else {
        return;
    }
    port_mode_apply();
    port_mode_persist();
}

void port_mode_toggle_port1_llamatron(void) {
    if (g_port1_mode == PORT1_MODE_LLAMA) {
        g_port1_mode = PORT1_MODE_MOUSE;
    } else if (port1_is_mouse_mode(g_port1_mode) || g_port1_mode == PORT1_MODE_JOY) {
        g_port1_mode = PORT1_MODE_LLAMA;
    }
    port_mode_apply();
    port_mode_persist();
    printf("[PORT] Port 1 mode: %s\n", port_mode_port1_label());
}

void port_mode_toggle_port2_cd32(void) {
    g_port2_cd32 = !g_port2_cd32;
    port_mode_apply();
    port_mode_persist();
    printf("[PORT] Port 2 mode: %s\n", port_mode_port2_label());
}

void port_mode_set_port2_cd32(bool enabled) {
    if (g_port2_cd32 == enabled) {
        return;
    }
    g_port2_cd32 = enabled;
    port_mode_apply();
    port_mode_persist();
}

#else

void port_mode_init(void) {}
port1_mode_t port_mode_get_port1(void) { return PORT1_MODE_MOUSE; }
bool port_mode_get_port2_cd32(void) { return false; }
const char* port_mode_port1_label(void) { return "Ami Ms"; }
const char* port_mode_port2_label(void) { return "Joy"; }
void port_mode_cycle_port1(void) {}
void port_mode_cycle_port2(void) {}
void port_mode_toggle_port1_mouse_joy(void) {}
void port_mode_toggle_port1_llamatron(void) {}
void port_mode_toggle_port2_cd32(void) {}
void port_mode_set_port2_cd32(bool enabled) { (void)enabled; }

#endif
