/**
 * this file is part of amigahid-pico, (c) 2024
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluepad32 initialization header
 */

#ifndef _BLUEPAD32_INIT_H
#define _BLUEPAD32_INIT_H

#if ENABLE_BLUEPAD32

#include <pico/async_context_poll.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Bluepad32 and return the async context (or NULL on failure)
async_context_poll_t* bluepad32_init(void);

// Poll btstack async_context (non-blocking, call from main loop)
void bluepad32_poll(void);

// Runtime control functions
void bluepad32_enable(void);
void bluepad32_disable(void);
bool bluepad32_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_INIT_H

