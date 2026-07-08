/**
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

static void port_mode_persist(void) {
    port_config_data_t config;
    config.mouse_type = amiga_quad_mouse_get_type();
    config.port1_mode = g_port1_mode;
    config.port2_cd32 = g_port2_cd32;
    port_config_save(&config);
}

static void port_mode_apply(void) {
    bool want_joy = (g_port1_mode != PORT1_MODE_MOUSE);
    bool want_llama = (g_port1_mode == PORT1_MODE_LLAMA);
    bool want_port1_cd32 = (g_port1_mode == PORT1_MODE_CD32);
    bool want_port2_cd32 = g_port2_cd32;

    if (want_port1_cd32 || want_port2_cd32) {
        want_llama = false;
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
        case PORT1_MODE_JOY: return "JOY";
        case PORT1_MODE_LLAMA: return "LTRON";
        case PORT1_MODE_CD32: return "CD32";
        case PORT1_MODE_MOUSE:
        default: return "MOUSE";
    }
}

void port_mode_cycle_port1(void) {
    switch (g_port1_mode) {
        case PORT1_MODE_MOUSE: g_port1_mode = PORT1_MODE_JOY; break;
        case PORT1_MODE_JOY: g_port1_mode = PORT1_MODE_LLAMA; break;
        case PORT1_MODE_LLAMA: g_port1_mode = PORT1_MODE_CD32; break;
        case PORT1_MODE_CD32:
        default: g_port1_mode = PORT1_MODE_MOUSE; break;
    }
    port_mode_apply();
    port_mode_persist();
    printf("[PORT] Port 1 mode: %s\n", port_mode_port1_label());
}

void port_mode_toggle_port1_mouse_joy(void) {
    if (g_port1_mode == PORT1_MODE_MOUSE) {
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
    } else if (g_port1_mode == PORT1_MODE_MOUSE || g_port1_mode == PORT1_MODE_JOY) {
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
    printf("[PORT] Port 2 CD32: %s\n", g_port2_cd32 ? "ON" : "OFF");
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
const char* port_mode_port1_label(void) { return "MOUSE"; }
void port_mode_cycle_port1(void) {}
void port_mode_toggle_port1_mouse_joy(void) {}
void port_mode_toggle_port1_llamatron(void) {}
void port_mode_toggle_port2_cd32(void) {}
void port_mode_set_port2_cd32(bool enabled) { (void)enabled; }

#endif
