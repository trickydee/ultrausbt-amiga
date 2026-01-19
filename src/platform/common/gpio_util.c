/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * shared GPIO utility functions for active-low signal control
 * 
 * Optimized version with direction state caching
 */

#include "gpio_util.h"
#include <hardware/gpio.h>
#include <stdint.h>
#include <stdbool.h>

// Cache GPIO direction state (bitmap: 1 = output, 0 = input or unknown)
// Using a simple array for GPIO 0-31 (32 pins max on RP2040)
// Each bit represents whether we've cached the direction for that GPIO
#define MAX_GPIO 32
static uint32_t gpio_dir_cache = 0;  // Bitmap: bit N = 1 if GPIO N is cached as OUTPUT

void amiga_gpio_set_active_low(uint32_t gpio, bool active)
{
    if (gpio >= MAX_GPIO) {
        return;  // Invalid GPIO
    }
    
    if (active) {
        // Active: set to LOW (0) and configure as output
        gpio_put(gpio, 0);
        // Only set direction if not already cached as output
        if (!(gpio_dir_cache & (1U << gpio))) {
            gpio_set_dir(gpio, GPIO_OUT);
            gpio_dir_cache |= (1U << gpio);  // Mark as output in cache
        }
    } else {
        // Inactive: set to HIGH (1) by setting as input (pulled high)
        // Only set direction if not already cached as input
        if (gpio_dir_cache & (1U << gpio)) {
            gpio_set_dir(gpio, GPIO_IN);
            gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
        }
    }
}

void amiga_gpio_init_active_low(uint32_t gpio, bool initial_active)
{
    if (gpio >= MAX_GPIO) {
        return;  // Invalid GPIO
    }
    
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    
    // Set initial state and cache direction
    if (initial_active) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_dir_cache |= (1U << gpio);  // Mark as output in cache
    } else {
        gpio_set_dir(gpio, GPIO_IN);
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
    }
}

void amiga_gpio_clear_cache(uint32_t gpio)
{
    if (gpio < MAX_GPIO) {
        gpio_dir_cache &= ~(1U << gpio);  // Clear cache for this GPIO
    }
}

