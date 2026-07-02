/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga quadrature mouse interface.
 */

#include "config.h"
#include "quad_mouse.h"
#include "platform/common/gpio_util.h"
#include "platform/amiga/joystick_port1.h"  // For checking joystick mode
#include "platform/amiga/cd32_pad.h"
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>  // For abs()

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "pico/flash.h"  // For flash_safe_execute_core_init() - required for Bluetooth flash coordination
#include "hardware/gpio.h"
#include "hardware/sync.h"  // For memory barriers (__dmb)

// mouse motion values, used between core0 and core1
volatile int8_t x = 0, y = 0;
volatile bool motion_flag = false;

// Core 1 pause flag - set to true to pause mouse processing (e.g., during Bluetooth enumeration)
volatile bool g_core1_paused = false;

// Core 1 heartbeat counter - increments every loop to detect if Core 1 is running
volatile uint32_t g_core1_heartbeat = 0;

// Mouse type: Amiga (default) or Atari (swapped pins 1 and 4)
// Will be loaded from flash on init, default to Amiga if not found
volatile mouse_type_t g_mouse_type = MOUSE_TYPE_AMIGA;

// Update periods based on reference implementations
// Amiga: 170μs tested in Yaumataca (leads to ~704μs actual period)
// Atari: 450μs tested in Yaumataca (leads to ~2ms actual period, more stable)
#define AMIGA_UPDATE_PERIOD_US 170
#define ATARI_UPDATE_PERIOD_US 450

// Mouse speed multiplier (1.0 = normal, higher = faster)
// Can be adjusted for different mouse sensitivities
#define MOUSE_SPEED_MULTIPLIER 1.0

// Configuration parameters (based on Atari adapter approach)
#define MOUSE_SDIV 1      // DPI division factor
#define MOUSE_SMUL 2      // Counter multiplier
#define MOUSE_MINF 0      // Minimum increment
#define MOUSE_MAXF 250    // Maximum increment

// Forward declarations for GPIO pin selection based on mouse type
static uint32_t get_gpio_v(void);
static uint32_t get_gpio_hq(void);

void amiga_quad_mouse_init()
{
    // Load mouse type from flash storage (persistent configuration)
    extern mouse_type_t mouse_config_load(void);
    mouse_type_t saved_type = mouse_config_load();
    __sync_synchronize();
    g_mouse_type = saved_type;
    __sync_synchronize();
    
    // obtain the pins we want to use
    gpio_init(QM1_AMIGA_H);
    gpio_init(QM1_AMIGA_V);
    gpio_init(QM1_AMIGA_HQ);
    gpio_init(QM1_AMIGA_VQ);
    gpio_init(QM1_AMIGA_B1);
    gpio_init(QM1_AMIGA_B2);
    gpio_init(QM1_AMIGA_B3);

    gpio_set_function(QM1_AMIGA_H, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_V, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_HQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_VQ, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B1, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B2, GPIO_FUNC_SIO);
    gpio_set_function(QM1_AMIGA_B3, GPIO_FUNC_SIO);

    // pins are active low, so when they are at 0 they're triggering; set all high (off)
    // Use optimized shared GPIO utility
    amiga_gpio_init_active_low(QM1_AMIGA_H, false);   // No horizontal direction
    amiga_gpio_init_active_low(QM1_AMIGA_V, false);   // No vertical direction
    amiga_gpio_init_active_low(QM1_AMIGA_HQ, false);  // No horizontal quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_VQ, false);  // No vertical quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_B1, false);  // Fire button not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B2, false);  // Button 2 not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B3, false);  // Button 3 not pressed

    // start the mouse motion loop on core1
    multicore_launch_core1(amiga_quad_mouse_motion);
}

void amiga_quad_mouse_button(enum amiga_quad_mouse_buttons button, bool pressed)
{
#if HIDPICO_REVISION == 5
    if (cd32_port1_is_enabled()) {
        return;
    }
#endif
    // Set GPIO state immediately - don't wait for state changes
    // This ensures buttons respond instantly, not just on state transitions
    switch (button) {
        case AQM_LEFT:      amiga_gpio_set_active_low(QM1_AMIGA_B1, pressed); break;
        case AQM_MIDDLE:    amiga_gpio_set_active_low(QM1_AMIGA_B3, pressed); break;
        case AQM_RIGHT:     amiga_gpio_set_active_low(QM1_AMIGA_B2, pressed); break;
        default:            break;
    }
}

void amiga_quad_mouse_set_motion(int8_t in_x, int8_t in_y)
{
    // Apply speed multiplier for acceleration support
    int8_t scaled_x = (int8_t)((double)in_x * MOUSE_SPEED_MULTIPLIER);
    int8_t scaled_y = (int8_t)((double)in_y * MOUSE_SPEED_MULTIPLIER);
    
    // Accumulate motion to handle rapid updates smoothly
    // This allows multiple small movements to be combined
    if (scaled_x != 0 || scaled_y != 0) {
        // Add to existing values (with overflow protection)
        int16_t new_x = (int16_t)x + (int16_t)scaled_x;
        int16_t new_y = (int16_t)y + (int16_t)scaled_y;
        
        // Clamp to int8_t range to prevent overflow
        if (new_x > 127) new_x = 127;
        if (new_x < -128) new_x = -128;
        if (new_y > 127) new_y = 127;
        if (new_y < -128) new_y = -128;
        
        x = (int8_t)new_x;
        y = (int8_t)new_y;
        motion_flag = true;
    }
}

// Get GPIO pin for V signal based on mouse type
// For Amiga: V -> DB-9 Pin 1 (normal)
// For Atari: HQ -> DB-9 Pin 1 (swapped with Pin 4)
static uint32_t get_gpio_v(void) {
    __sync_synchronize();
    mouse_type_t type = g_mouse_type;
    __sync_synchronize();
    if (type == MOUSE_TYPE_ATARI) {
        return QM1_AMIGA_HQ;  // Atari: HQ goes to DB-9 Pin 1
    }
    return QM1_AMIGA_V;  // Amiga: V goes to DB-9 Pin 1
}

// Get GPIO pin for HQ signal based on mouse type
// For Amiga: HQ -> DB-9 Pin 4 (normal)
// For Atari: V -> DB-9 Pin 4 (swapped with Pin 1)
static uint32_t get_gpio_hq(void) {
    __sync_synchronize();
    mouse_type_t type = g_mouse_type;
    __sync_synchronize();
    if (type == MOUSE_TYPE_ATARI) {
        return QM1_AMIGA_V;  // Atari: V goes to DB-9 Pin 4
    }
    return QM1_AMIGA_HQ;  // Amiga: HQ goes to DB-9 Pin 4
}

void amiga_quad_mouse_set_type(mouse_type_t type)
{
    __sync_synchronize();
    g_mouse_type = type;
    __sync_synchronize();
}

mouse_type_t amiga_quad_mouse_get_type(void)
{
    __sync_synchronize();
    mouse_type_t type = g_mouse_type;
    __sync_synchronize();
    return type;
}

void amiga_quad_mouse_toggle_type(void)
{
    __sync_synchronize();
    g_mouse_type = (g_mouse_type == MOUSE_TYPE_AMIGA) ? MOUSE_TYPE_ATARI : MOUSE_TYPE_AMIGA;
    __sync_synchronize();
    
    // Save to flash for persistence
    extern bool mouse_config_save(mouse_type_t mouse_type);
    mouse_config_save(g_mouse_type);
}

void amiga_quad_mouse_motion()
{
    // CRITICAL: Initialize flash-safe execution FIRST
    // This allows Core 0 to coordinate with Core 1 when Bluetooth writes to flash (TLV storage)
    // Without this, Core 1 can freeze when Bluetooth tries to access flash during pairing
    // This is required for proper flash coordination, especially for devices requiring SSP (Secure Simple Pairing)
    // Reference: Atari keyboard interface implementation
    flash_safe_execute_core_init();
    
    // 16-bit accumulator approach (based on Atari-Quadrature-USB-Mouse-Adapter)
    // Uses upper 8 bits for phase extraction, providing smoother transitions
    uint16_t xval = 0;  // 16-bit accumulator for X axis
    uint16_t yval = 0;  // 16-bit accumulator for Y axis
    uint8_t xph = 0;    // Current phase for X axis (extracted from xval >> 8)
    uint8_t yph = 0;    // Current phase for Y axis (extracted from yval >> 8)
    
    // Pulse counters and delta values
    uint8_t xcnt = 0;   // Remaining pulses for X axis
    uint8_t ycnt = 0;   // Remaining pulses for Y axis
    int16_t xdelta = 0; // Increment per update for X axis
    int16_t ydelta = 0; // Increment per update for Y axis
    
    // Track previous delta direction for direction change detection
    bool prev_x_neg = false;
    bool prev_y_neg = false;
    
    absolute_time_t last_update = get_absolute_time();
    
    /**
     * Quadrature motion uses a hardware-side counter with two signal lines per axis.
     * Motion is signalled in an offset time division; the main axis pulse changes
     * state on time 0 and time 1, and the second signal line at time interval 0.5
     * and 1.5, giving four possible states for each t/2.
     * 
     * This implementation uses 16-bit accumulators with phase extraction from
     * upper 8 bits, providing smooth transitions and proper direction change handling.
     * 
     * Reference: Atari-Quadrature-USB-Mouse-Adapter and Yaumataca projects
     */

    while (1) {
        // Increment heartbeat counter FIRST to show Core 1 is always running
        g_core1_heartbeat++;
        
        // Check if Core 1 is paused (e.g., during Bluetooth enumeration)
        __sync_synchronize();
        bool paused = g_core1_paused;
        __sync_synchronize();
        
        if (paused) {
            busy_wait_us(5000);
            continue;
        }
        
        absolute_time_t current_time = get_absolute_time();
        
        // Get update period based on mouse type
        __sync_synchronize();
        mouse_type_t mouse_type = g_mouse_type;
        __sync_synchronize();
        uint32_t update_period_us = (mouse_type == MOUSE_TYPE_ATARI) ? ATARI_UPDATE_PERIOD_US : AMIGA_UPDATE_PERIOD_US;
        
        // Check if it's time to update
        int64_t time_diff = absolute_time_diff_us(last_update, current_time);
        if (time_diff >= (int64_t)update_period_us) {
            last_update = current_time;
            
            // Process new motion input
            if (motion_flag) {
        // Read motion atomically
                int8_t new_x = x;
                int8_t new_y = y;
        x = y = 0;
        motion_flag = false;

                // Apply speed multiplier
                int8_t scaled_x = (int8_t)((double)new_x * MOUSE_SPEED_MULTIPLIER);
                int8_t scaled_y = (int8_t)((double)new_y * MOUSE_SPEED_MULTIPLIER);
                
                // Process X axis movement
                if (scaled_x != 0) {
                    bool neg_mvmt = (scaled_x < 0);
                    uint8_t abs_mvmt = neg_mvmt ? -scaled_x : scaled_x;
                    abs_mvmt /= MOUSE_SDIV;
                    
                    // Direction change handling (prevents bounce-back)
                    if (xcnt && (neg_mvmt != prev_x_neg)) {
                        // Direction changed while movement pending
                        if (xcnt >= abs_mvmt) {
                            xcnt -= abs_mvmt;
                            prev_x_neg = !prev_x_neg;
                        } else {
                            xcnt = abs_mvmt - xcnt;
                            prev_x_neg = neg_mvmt;
                        }
                    } else {
                        // Normal accumulation with saturation
                        uint16_t new_xcnt = xcnt + abs_mvmt;
                        if (new_xcnt > 255) new_xcnt = 255;
                        xcnt = (uint8_t)new_xcnt;
                        prev_x_neg = neg_mvmt;
                    }
                    
                    // Calculate delta (speed-proportional)
                    uint16_t delta = MOUSE_MINF + MOUSE_SMUL * xcnt;
                    if (delta > MOUSE_MAXF) delta = MOUSE_MAXF;
                    xdelta = prev_x_neg ? -(int16_t)delta : (int16_t)delta;
                } else {
                    xdelta = 0;
                }
                
                // Process Y axis movement
                if (scaled_y != 0) {
                    bool neg_mvmt = (scaled_y < 0);
                    uint8_t abs_mvmt = neg_mvmt ? -scaled_y : scaled_y;
                    abs_mvmt /= MOUSE_SDIV;
                    
                    // Direction change handling
                    if (ycnt && (neg_mvmt != prev_y_neg)) {
                        if (ycnt >= abs_mvmt) {
                            ycnt -= abs_mvmt;
                            prev_y_neg = !prev_y_neg;
                        } else {
                            ycnt = abs_mvmt - ycnt;
                            prev_y_neg = neg_mvmt;
                        }
                    } else {
                        uint16_t new_ycnt = ycnt + abs_mvmt;
                        if (new_ycnt > 255) new_ycnt = 255;
                        ycnt = (uint8_t)new_ycnt;
                        prev_y_neg = neg_mvmt;
                    }
                    
                    uint16_t delta = MOUSE_MINF + MOUSE_SMUL * ycnt;
                    if (delta > MOUSE_MAXF) delta = MOUSE_MAXF;
                    ydelta = prev_y_neg ? -(int16_t)delta : (int16_t)delta;
                } else {
                    ydelta = 0;
                }
            }
            
            // Update accumulators and extract phase
            // NOTE: Allow natural wraparound (no clamping) - matches reference implementation
            // When accumulator wraps from 65535->0 or 0->65535, phase still changes correctly
            if (xdelta != 0 || xcnt > 0) {
                // Continue processing as long as xcnt > 0, even if xdelta == 0
                // This allows the accumulator to keep updating and xcnt to decrement
                if (xdelta != 0) {
                    // Add delta directly - allow natural uint16_t wraparound
                    xval = (uint16_t)((int32_t)xval + (int32_t)xdelta);
                }
                
                // Extract phase from upper 8 bits
                uint8_t new_xph = (xval >> 8) & 0xFF;
                if (new_xph != xph) {
                    xph = new_xph;
                    if (xcnt > 0) xcnt--;
                    
                    // Update GPIO based on phase (quadrature encoding)
                    // Quadrature lookup tables (from Yaumataca reference)
                    // lut_a: {0, 1, 1, 0} - Signal A
                    // lut_b: {0, 0, 1, 1} - Signal B (90° shifted)
                    __sync_synchronize();
                    bool joy_mode = amiga_joystick_port1_is_joystick_mode();
#if HIDPICO_REVISION == 5
                    bool port1_cd32 = cd32_port1_is_enabled();
#else
                    bool port1_cd32 = false;
#endif
                    __sync_synchronize();
                    if (!joy_mode && !port1_cd32) {
                        uint32_t gpio_hq = get_gpio_hq();
                        uint8_t quad_state = xph & 0x03;
                        // Quadrature lookup tables
                        const bool lut_a[4] = {0, 1, 1, 0};
                        const bool lut_b[4] = {0, 0, 1, 1};
                        amiga_gpio_set_active_low(QM1_AMIGA_H, lut_a[quad_state]);
                        amiga_gpio_set_active_low(gpio_hq, lut_b[quad_state]);
                    }
                }
                if (xcnt == 0) xdelta = 0;
            }
            
            if (ydelta != 0 || ycnt > 0) {
                // Continue processing as long as ycnt > 0, even if ydelta == 0
                // This allows the accumulator to keep updating and ycnt to decrement
                if (ydelta != 0) {
                    // Add delta directly - allow natural uint16_t wraparound
                    yval = (uint16_t)((int32_t)yval + (int32_t)ydelta);
                }
                
                uint8_t new_yph = (yval >> 8) & 0xFF;
                if (new_yph != yph) {
                    yph = new_yph;
                    if (ycnt > 0) ycnt--;
                    
                    __sync_synchronize();
                    bool joy_mode = amiga_joystick_port1_is_joystick_mode();
#if HIDPICO_REVISION == 5
                    bool port1_cd32 = cd32_port1_is_enabled();
#else
                    bool port1_cd32 = false;
#endif
                    __sync_synchronize();
                    if (!joy_mode && !port1_cd32) {
                        uint32_t gpio_v = get_gpio_v();
                        uint8_t quad_state = yph & 0x03;
                        // Quadrature lookup tables
                        const bool lut_a[4] = {0, 1, 1, 0};
                        const bool lut_b[4] = {0, 0, 1, 1};
                        amiga_gpio_set_active_low(gpio_v, lut_a[quad_state]);
                        amiga_gpio_set_active_low(QM1_AMIGA_VQ, lut_b[quad_state]);
                    }
                }
                if (ycnt == 0) ydelta = 0;
            }
        }
        
        // Sleep briefly to avoid busy-waiting
        sleep_us(50);
    }
}

// Core 1 pause/resume functions for Bluetooth enumeration coordination
void amiga_quad_mouse_pause_core1(void)
{
    // Use atomic store with memory barrier to ensure Core 1 sees the change
    __sync_synchronize();
    g_core1_paused = true;
    __sync_synchronize();
    // Force a memory write barrier to ensure the write is visible to Core 1
    __dmb();
}

void amiga_quad_mouse_resume_core1(void)
{
    // Use atomic store with memory barrier to ensure Core 1 sees the change
    __sync_synchronize();
    g_core1_paused = false;
    __sync_synchronize();
    // Force a memory write barrier to ensure the write is visible to Core 1
    __dmb();
    // Add a small delay to ensure Core 1 has time to see the change
    // This is especially important if Core 1 is in a tight loop
    busy_wait_us(50);  // 50us delay (reduced from 100us for faster resume)
}
