/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * shared GPIO utility functions for active-low signal control
 * 
 * Optimized version that caches GPIO direction state to avoid redundant
 * gpio_set_dir() calls, which improves performance in hot paths.
 */

#ifndef _PLATFORM_COMMON_GPIO_UTIL_H
#define _PLATFORM_COMMON_GPIO_UTIL_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Set GPIO pin for active-low signal control with direction caching
 * 
 * This optimized version caches the GPIO direction state to avoid
 * redundant gpio_set_dir() calls when the direction hasn't changed.
 * 
 * @param gpio GPIO pin number
 * @param active true = signal active (LOW/0), false = signal inactive (HIGH/1 via input mode)
 */
void amiga_gpio_set_active_low(uint32_t gpio, bool active);

/**
 * Initialize GPIO pin for active-low signal control
 * Sets initial state and caches direction
 * 
 * @param gpio GPIO pin number
 * @param initial_active Initial state (true = active/LOW, false = inactive/HIGH)
 */
void amiga_gpio_init_active_low(uint32_t gpio, bool initial_active);

/**
 * Clear GPIO direction cache (call after changing GPIO function)
 * Use this if you need to change GPIO function and want to reset the cache
 */
void amiga_gpio_clear_cache(uint32_t gpio);

/**
 * Clear all GPIO direction cache
 * Call this before initialization to ensure clean state
 */
void amiga_gpio_clear_all_cache(void);

/**
 * Reset all Amiga GPIOs to INPUT state (inactive/high)
 * Call this before initialization, especially if Amiga is already powered
 * This ensures clean state even if Amiga's pull-ups have already pulled lines high
 */
void amiga_gpio_reset_all_to_input(void);

/**
 * Watchdog: Check if GPIOs are stuck in wrong state and recover if needed
 * This is a lightweight check that should be called periodically (e.g., every 5 seconds)
 * Returns true if recovery was needed, false otherwise
 */
bool amiga_gpio_watchdog_check(void);

#endif // _PLATFORM_COMMON_GPIO_UTIL_H

