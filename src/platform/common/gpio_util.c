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
#include "util/output.h"
#if HIDPICO_REV_ATARI_BOARD
#include "platform/amiga/cd32_pad.h"
#endif
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
        // IMPORTANT: For GPIOs that are also ADC inputs (26=ADC0, 27=ADC1, 28=ADC2), 
        // ensure ADC is not interfering with GPIO operation
        // ADC can prevent proper LOW output if enabled
        if (gpio == 26 || gpio == 27 || gpio == 28) {
            // GPIO 26 = ADC0, GPIO 27 = ADC1, GPIO 28 = ADC2
            // Ensure GPIO function is set (not ADC) - this disables ADC on these pins
            gpio_set_function(gpio, GPIO_FUNC_SIO);  // Ensure GPIO function (not ADC)
            __sync_synchronize();  // Ensure function change is visible
        }
        
        // IMPORTANT: Disable pull-ups FIRST (pull-ups can prevent LOW output on some GPIOs)
        gpio_set_pulls(gpio, false, false);  // Disable both pull-up and pull-down
        __sync_synchronize();  // Ensure pull-up disable is visible
        
        // Set direction to OUTPUT
        gpio_set_dir(gpio, GPIO_OUT);
        // Small delay to ensure direction is set before level (helps with cross-core timing)
        __sync_synchronize();
        
        // Set level to LOW
        gpio_put(gpio, 0);
        // Small delay to ensure level is set (some GPIOs need time to settle)
        __sync_synchronize();
        gpio_dir_cache |= (1U << gpio);  // Mark as output in cache
    } else {
        // Inactive: set to HIGH (1)
#if ENABLE_LEVEL_SHIFTER
        // Level shifter mode: Set as INPUT with pull-up (3.3V)
        // TXB0108 requires pull-ups on BOTH sides (A-side 3.3V and B-side 5V) for proper operation
        // This matches the original BSS138 design which had pull-ups on both sides
        // The 3.3V pull-up works with the 5V pull-up to ensure proper bidirectional operation
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, true, false);  // Enable pull-up (3.3V) - required for TXB0108
        __sync_synchronize();
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
#else
        // Direct connection mode: Set as INPUT with pull-up (3.3V)
        // This works when directly connected to Amiga (no level shifter)
        // Always set direction to INPUT FIRST
        // This ensures the GPIO is in the correct state even if mouse code changed it
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, true, false);  // Enable pull-up, disable pull-down
        // Small delay to ensure direction is set (helps with cross-core timing)
        __sync_synchronize();
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
#endif
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
        // Active: set to LOW output
        // IMPORTANT: For GPIOs that are also ADC inputs (26=ADC0, 27=ADC1, 28=ADC2), 
        // ensure ADC is not interfering
        if (gpio == 26 || gpio == 27 || gpio == 28) {
            gpio_set_function(gpio, GPIO_FUNC_SIO);  // Ensure GPIO function (not ADC)
            __sync_synchronize();  // Ensure function change is visible
        }
        gpio_set_pulls(gpio, false, false);  // Disable pull-ups/pull-downs for OUTPUT
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, 0);
        gpio_dir_cache |= (1U << gpio);  // Mark as output in cache
    } else {
        // Inactive: set to HIGH state
#if ENABLE_LEVEL_SHIFTER
        // Level shifter mode: Set as INPUT with pull-up (3.3V)
        // TXB0108 requires pull-ups on BOTH sides (A-side 3.3V and B-side 5V) for proper operation
        // This matches the original BSS138 design which had pull-ups on both sides
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, true, false);  // Enable pull-up (3.3V) - required for TXB0108
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
#else
        // Direct connection mode: Set as INPUT with pull-up (3.3V)
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, true, false);  // Enable pull-up, disable pull-down
        gpio_dir_cache &= ~(1U << gpio);  // Mark as input in cache
#endif
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
#if HIDPICO_REV_ATARI_BOARD
    // Clear all GPIO direction cache first
    amiga_gpio_clear_all_cache();
    
    // Reset all Amiga joystick/mouse GPIOs to INPUT (inactive/high) state
    // This ensures clean state even if Amiga is already powered and pull-ups are active
    // IMPORTANT: All GPIOs must be INPUT with pull-up at startup to prevent back-feeding
    // 5V from the Amiga when the Pico is not powered or during power-up
    // This is CRITICAL for hardware protection - OUTPUT GPIOs can be damaged by 5V back-feeding
    
    // Port 1 / Mouse GPIOs (GPIOs 10-14, 2-3)
    amiga_gpio_init_active_low(QM1_AMIGA_H, false);   // GPIO 11 - No horizontal direction
    amiga_gpio_init_active_low(QM1_AMIGA_V, false);   // GPIO 10 - No vertical direction
    amiga_gpio_init_active_low(QM1_AMIGA_HQ, false);  // GPIO 13 - No horizontal quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_VQ, false);  // GPIO 12 - No vertical quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_B1, false);  // GPIO 14 - Fire button not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B2, false);  // GPIO 2 - Button 2 not pressed (remapped)
    amiga_gpio_init_active_low(QM1_AMIGA_B3, false);  // GPIO 3 - Button 3 not pressed (remapped)
    
    // Port 2 GPIOs (directions + fire/B2/B3 per config.h — Rev 6: 7/0/1, Rev 5: 26/27/28)
    amiga_gpio_init_active_low(QM2_AMIGA_H, false);
    amiga_gpio_init_active_low(QM2_AMIGA_V, false);
    amiga_gpio_init_active_low(QM2_AMIGA_HQ, false);
    amiga_gpio_init_active_low(QM2_AMIGA_VQ, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B1, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B2, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B3, false);
    
    // All GPIOs are now in INPUT mode with pull-up enabled (safe state)
    // They will be set to OUTPUT only when actively driving a signal LOW
    // This prevents 5V back-feeding damage when Amiga is powered but Pico is not
#endif
}

bool amiga_gpio_watchdog_check(void)
{
#if HIDPICO_REV_ATARI_BOARD
    if (cd32_port1_is_enabled() || cd32_port2_is_enabled()) {
        return false;
    }

    // Lightweight watchdog: Check a sample of GPIOs to detect stuck states
    // We check a few representative GPIOs rather than all to minimize performance impact
    // 
    // IMPORTANT: This watchdog only detects GPIOs stuck in OUTPUT when they should be INPUT.
    // It does NOT reset GPIOs during normal operation to avoid interrupting USB/Bluetooth.
    // Only truly stuck states (GPIO is OUTPUT but cache says INPUT AND GPIO value is LOW)
    // will trigger recovery.
    
    // Sample GPIOs to check (representative GPIOs from each port)
    // NOTE: We EXCLUDE buttons 2 and 3 from watchdog checks to prevent interference
    // Buttons 2 and 3 may be actively pressed and should not be reset by watchdog
    const uint32_t sample_gpios[] = {
        QM1_AMIGA_V,   // Port 1 UP direction
        QM1_AMIGA_B1,  // Port 1 Fire button
        // QM1_AMIGA_B2,  // Port 1 Button 2 (GPIO 2) - EXCLUDED from watchdog
        // QM1_AMIGA_B3,  // Port 1 Button 3 (GPIO 3) - EXCLUDED from watchdog
        QM2_AMIGA_V,   // Port 2 UP direction
        QM2_AMIGA_B1,  // Port 2 Fire
        // QM2_AMIGA_B2,  // Port 2 Button 2 - EXCLUDED from watchdog
        // QM2_AMIGA_B3,  // Port 2 Button 3 - EXCLUDED from watchdog
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
        // NOTE: Buttons 2 and 3 are already excluded from sample_gpios[], so this check is redundant
        // but kept for safety in case sample_gpios[] is modified in the future
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
            // Reset GPIOs to INPUT (inactive) state, but PRESERVE buttons 2 and 3
            // Buttons 2 and 3 may be actively pressed and should not be reset
            // Save current state of buttons 2 and 3 before reset
            // Check if buttons are actually pressed (GPIO is OUTPUT and LOW)
            bool b2_state = (gpio_get_dir(QM1_AMIGA_B2) == GPIO_OUT) && !gpio_get(QM1_AMIGA_B2);
            bool b3_state = (gpio_get_dir(QM1_AMIGA_B3) == GPIO_OUT) && !gpio_get(QM1_AMIGA_B3);
            bool b2_port2_state = (gpio_get_dir(QM2_AMIGA_B2) == GPIO_OUT) && !gpio_get(QM2_AMIGA_B2);
            bool b3_port2_state = (gpio_get_dir(QM2_AMIGA_B3) == GPIO_OUT) && !gpio_get(QM2_AMIGA_B3);
            
            // Reset all GPIOs to INPUT (inactive) state
            // This will fix any stuck GPIO states
            amiga_gpio_reset_all_to_input();
            
            // Restore buttons 2 and 3 if they were pressed
            // Only restore if they were actually active (LOW/OUTPUT)
            if (b2_state && gpio_get_dir(QM1_AMIGA_B2) == GPIO_OUT) {
                amiga_gpio_set_active_low(QM1_AMIGA_B2, true);
            }
            if (b3_state && gpio_get_dir(QM1_AMIGA_B3) == GPIO_OUT) {
                amiga_gpio_set_active_low(QM1_AMIGA_B3, true);
            }
            if (b2_port2_state && gpio_get_dir(QM2_AMIGA_B2) == GPIO_OUT) {
                amiga_gpio_set_active_low(QM2_AMIGA_B2, true);
            }
            if (b3_port2_state && gpio_get_dir(QM2_AMIGA_B3) == GPIO_OUT) {
                amiga_gpio_set_active_low(QM2_AMIGA_B3, true);
            }
            
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

