/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Mouse and port configuration persistence using flash storage.
 */

#ifndef _PLATFORM_AMIGA_MOUSE_CONFIG_H
#define _PLATFORM_AMIGA_MOUSE_CONFIG_H

#include "quad_mouse.h"
#include "port_mode.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    mouse_type_t mouse_type;
    port1_mode_t port1_mode;
    bool port2_cd32;
    uint8_t usb_device_mode;  // 0 = normal (USB->Amiga host), 1 = PC keyboard (Amiga->USB device)
} port_config_data_t;

mouse_type_t mouse_config_load(void);
bool mouse_config_save(mouse_type_t mouse_type);

void port_config_load(port_config_data_t* out);
bool port_config_save(const port_config_data_t* config);
/** Always write now (never defer). Use for USB role changes before reboot. */
bool port_config_save_immediate(const port_config_data_t* config);
void port_config_flush_pending(void);

#endif
