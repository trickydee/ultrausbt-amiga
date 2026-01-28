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
#include "config.h"
#include <hardware/gpio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>  // For printf in watchdog

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
        // Always set direction to OUTPUT FIRST, then set level
        // This ensures the GPIO is in the correct state even if mouse code changed it
        gpio_set_dir(gpio, GPIO_OUT);
        // Small delay to ensure direction is set before level (helps with cross-core timing)
        __sync_synchronize();
        gpio_put(gpio, 0);
        gpio_dir_cache |= (1U << gpio);  // Mark as output in cache
    } else {
        // Inactive: set to HIGH (1) by setting as input (pulled high)
        // Always set direction to INPUT FIRST, then the pull-up will set it high
        // This ensures the GPIO is in the correct state even if mouse code changed it
        gpio_set_dir(gpio, GPIO_IN);
        // Small delay to ensure direction is set (helps with cross-core timing)
        __sync_synchronize();
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
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

void amiga_gpio_clear_all_cache(void)
{
    gpio_dir_cache = 0;  // Clear all cached GPIO directions
}

void amiga_gpio_reset_all_to_input(void)
{
#if HIDPICO_REVISION == 5
    // Clear all GPIO direction cache first
    amiga_gpio_clear_all_cache();
    
    // Reset all Amiga joystick/mouse GPIOs to INPUT (inactive/high) state
    // This ensures clean state even if Amiga is already powered and pull-ups are active
    
    // Port 1 / Mouse GPIOs
    amiga_gpio_init_active_low(QM1_AMIGA_H, false);   // No horizontal direction
    amiga_gpio_init_active_low(QM1_AMIGA_V, false);   // No vertical direction
    amiga_gpio_init_active_low(QM1_AMIGA_HQ, false);  // No horizontal quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_VQ, false);  // No vertical quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_B1, false);  // Fire button not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B2, false);  // Button 2 not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B3, false);  // Button 3 not pressed
    
    // Port 2 GPIOs
    amiga_gpio_init_active_low(QM2_AMIGA_H, false);   // No horizontal direction
    amiga_gpio_init_active_low(QM2_AMIGA_V, false);   // No vertical direction
    amiga_gpio_init_active_low(QM2_AMIGA_HQ, false);  // No horizontal quadrature
    amiga_gpio_init_active_low(QM2_AMIGA_VQ, false);  // No vertical quadrature
    amiga_gpio_init_active_low(QM2_AMIGA_B1, false);  // Fire button not pressed
    amiga_gpio_init_active_low(QM2_AMIGA_B2, false);  // Button 2 not pressed
    amiga_gpio_init_active_low(QM2_AMIGA_B3, false);  // Button 3 not pressed
#endif
}

bool amiga_gpio_watchdog_check(void)
{
#if HIDPICO_REVISION == 5
    // Lightweight watchdog: Check a sample of GPIOs to detect stuck states
    // We check a few representative GPIOs rather than all to minimize performance impact
    // 
    // IMPORTANT: This watchdog only detects GPIOs stuck in OUTPUT when they should be INPUT.
    // It does NOT reset GPIOs during normal operation to avoid interrupting USB/Bluetooth.
    // Only truly stuck states (GPIO is OUTPUT but cache says INPUT AND GPIO value is LOW)
    // will trigger recovery.
    
    // Sample GPIOs to check (one from each port)
    const uint32_t sample_gpios[] = {
        QM1_AMIGA_V,   // Port 1 UP direction
        QM1_AMIGA_B1,  // Port 1 Fire button
        QM2_AMIGA_V,   // Port 2 UP direction
        QM2_AMIGA_B1,  // Port 2 Fire button
    };
    const int sample_count = sizeof(sample_gpios) / sizeof(sample_gpios[0]);
    
    bool recovery_needed = false;
    static uint32_t consecutive_mismatches = 0;  // Debounce: require multiple consecutive mismatches
    
    // Check if any sampled GPIO is stuck in OUTPUT when it should be INPUT
    // Only trigger if: GPIO is OUTPUT (hardware) AND cache says INPUT AND GPIO value is LOW (stuck active)
    for (int i = 0; i < sample_count; i++) {
        uint32_t gpio = sample_gpios[i];
        if (gpio >= MAX_GPIO) continue;
        
        // Read actual GPIO direction and value from hardware
        bool is_output = gpio_get_dir(gpio);
        bool cached_as_output = (gpio_dir_cache & (1U << gpio)) != 0;
        bool gpio_value = gpio_get(gpio);
        
        // Only consider it stuck if:
        // 1. GPIO is OUTPUT (hardware)
        // 2. Cache says it should be INPUT
        // 3. GPIO value is LOW (stuck in active state)
        // This avoids false positives during normal button presses
        if (is_output && !cached_as_output && !gpio_value) {
            recovery_needed = true;
            break;
        }
    }
    
    // Debounce: require 3 consecutive checks with mismatches before recovering
    // This prevents recovery during normal GPIO transitions
    if (recovery_needed) {
        consecutive_mismatches++;
        if (consecutive_mismatches >= 3) {
            // Reset all GPIOs to INPUT (inactive) state
            // This will fix any stuck GPIO states
            amiga_gpio_reset_all_to_input();
            consecutive_mismatches = 0;  // Reset counter after recovery
            return true;  // Recovery was performed
        }
    } else {
        consecutive_mismatches = 0;  // Reset counter if no mismatch
    }
    
    return false;  // No recovery needed
#else
    return false;  // Watchdog only for Revision 5
#endif
}

