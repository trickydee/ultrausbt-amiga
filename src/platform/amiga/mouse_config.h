/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Mouse configuration persistence interface.
 */

#ifndef _PLATFORM_AMIGA_MOUSE_CONFIG_H
#define _PLATFORM_AMIGA_MOUSE_CONFIG_H

#include "quad_mouse.h"
#include <stdbool.h>

/**
 * Load mouse configuration from flash storage.
 * Returns the saved mouse type, or MOUSE_TYPE_AMIGA if no valid config found.
 */
mouse_type_t mouse_config_load(void);

/**
 * Save mouse configuration to flash storage.
 * Returns true on success, false on failure.
 */
bool mouse_config_save(mouse_type_t mouse_type);

#endif // _PLATFORM_AMIGA_MOUSE_CONFIG_H

