/**
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
} port_config_data_t;

mouse_type_t mouse_config_load(void);
bool mouse_config_save(mouse_type_t mouse_type);

void port_config_load(port_config_data_t* out);
bool port_config_save(const port_config_data_t* config);
void port_config_flush_pending(void);

#endif
