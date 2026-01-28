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

// Speed-proportional timing constants (based on Atari implementation)
#define MAX_SPEED 30000.0    // Maximum speed value for period calculation (reduced for better fast movement)
#define MIN_PERIOD_US 200    // Minimum period in microseconds (lowered to allow faster updates)
#define DEFAULT_PERIOD_US 3000  // Default period when no motion (for idle state)

// Mouse speed multiplier (1.0 = normal, higher = faster)
// Can be adjusted for different mouse sensitivities
#define MOUSE_SPEED_MULTIPLIER 1.0

void amiga_quad_mouse_init()
{
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
    // ahprintf("[aqm] button %s state %s\n",
    //     (button == AQM_LEFT) ? "left" :
    //         (button == AQM_MIDDLE) ? "middle" :
    //         (button == AQM_RIGHT) ? "right" : "<unknown?!>",
    //     pressed ? "down" : "up"
    // );

    switch (button) {
        case AQM_LEFT:      amiga_gpio_set_active_low(QM1_AMIGA_B1, pressed); break;
        case AQM_MIDDLE:    amiga_gpio_set_active_low(QM1_AMIGA_B3, pressed); break;
        case AQM_RIGHT:     amiga_gpio_set_active_low(QM1_AMIGA_B2, pressed); break;
        // default:            ahprintf("[aqm] unhandled button press!\n");
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

void amiga_quad_mouse_motion()
{
    // CRITICAL: Initialize flash-safe execution FIRST
    // This allows Core 0 to coordinate with Core 1 when Bluetooth writes to flash (TLV storage)
    // Without this, Core 1 can freeze when Bluetooth tries to access flash during pairing
    // This is required for proper flash coordination, especially for devices requiring SSP (Secure Simple Pairing)
    // Reference: Atari keyboard interface implementation
    flash_safe_execute_core_init();
    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    uint8_t quad_mx_state = 0, quad_my_state = 0;
    
    // Time-based quadrature generation (based on Atari implementation)
    // Track timing for each axis independently
    absolute_time_t last_x_time = get_absolute_time();
    absolute_time_t last_y_time = get_absolute_time();
    int32_t x_period_us = 0;  // Period in microseconds for X axis (0 = no motion, negative = left, positive = right)
    int32_t y_period_us = 0;  // Period in microseconds for Y axis (0 = no motion, negative = up, positive = down)
    int8_t remaining_x = 0;   // Remaining X motion to process
    int8_t remaining_y = 0;   // Remaining Y motion to process

    /**
     * a little note about quadrature motion state.
     *
     * quadrature motion works by having a hardware-side counter for each axis and two signal
     * lines per axis. motion is signalled in an offset time division; the main axis pulse
     * changes state on time 0 and time 1, and the second signal line at time interval 0.5 and
     * 1.5, giving four possible states for each t/2. this occurs on both x and y axis.
     *
     * adcd has a crude ascii timing diagram but it explains it better:
     * https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017F.html
     * 
     * This implementation uses speed-proportional timing: faster movement = shorter period = 
     * more frequent quadrature updates, providing better responsiveness.
     */

    while (1) {
        // Increment heartbeat counter FIRST to show Core 1 is always running
        g_core1_heartbeat++;
        
        // Check if Core 1 is paused (e.g., during Bluetooth enumeration)
        // This prevents flash access conflicts during GATT service discovery
        // IMPORTANT: Read the flag with a memory barrier to ensure we see the latest value
        // Use __sync_synchronize() to ensure we read from memory, not cache
        __sync_synchronize();
        bool paused = g_core1_paused;
        __sync_synchronize();
        
        if (paused) {
            // When paused, use busy_wait instead of sleep_ms to ensure we check the flag frequently
            // This allows Core 1 to resume quickly when the flag changes
            busy_wait_us(5000);  // 5ms busy wait (reduced from 10ms for faster response)
            continue;
        }
        
        absolute_time_t current_time = get_absolute_time();
        
        // Check for new motion input
        if (motion_flag) {
            // Read motion atomically and accumulate
            remaining_x += x;
            remaining_y += y;
            x = y = 0;
            motion_flag = false;
            
            // Clamp accumulated motion to prevent overflow
            if (remaining_x > 127) remaining_x = 127;
            if (remaining_x < -128) remaining_x = -128;
            if (remaining_y > 127) remaining_y = 127;
            if (remaining_y < -128) remaining_y = -128;
            
            // Calculate period based on movement speed (speed-proportional timing)
            // Faster movement = shorter period = more responsive
            // Formula: period = MAX_SPEED / abs(speed), clamped to MIN_PERIOD_US
            if (remaining_x != 0) {
                int32_t speed = (int32_t)remaining_x;
                int32_t abs_speed = (speed < 0) ? -speed : speed;
                if (abs_speed > 0) {
                    x_period_us = (int32_t)(MAX_SPEED / (double)abs_speed);
                    if (x_period_us < MIN_PERIOD_US) {
                        x_period_us = MIN_PERIOD_US;
                    } else if (x_period_us > DEFAULT_PERIOD_US * 10) {
                        x_period_us = DEFAULT_PERIOD_US * 10;  // Cap very slow movements
                    }
                    if (speed < 0) x_period_us = -x_period_us;  // Negative for left movement
                } else {
                    x_period_us = 0;
                }
                last_x_time = current_time;
            }
            
            if (remaining_y != 0) {
                int32_t speed = (int32_t)remaining_y;
                int32_t abs_speed = (speed < 0) ? -speed : speed;
                if (abs_speed > 0) {
                    y_period_us = (int32_t)(MAX_SPEED / (double)abs_speed);
                    if (y_period_us < MIN_PERIOD_US) {
                        y_period_us = MIN_PERIOD_US;
                    } else if (y_period_us > DEFAULT_PERIOD_US * 10) {
                        y_period_us = DEFAULT_PERIOD_US * 10;  // Cap very slow movements
                    }
                    if (speed < 0) y_period_us = -y_period_us;  // Negative for up movement
                } else {
                    y_period_us = 0;
                }
                last_y_time = current_time;
            }
        }
        
        // Time-based quadrature state updates (only when motion is active)
        // Check if it's time to update X axis
        if (x_period_us != 0) {
            int64_t elapsed_x = absolute_time_diff_us(last_x_time, current_time);
            int32_t abs_period_x = (x_period_us < 0) ? -x_period_us : x_period_us;
            
            if (elapsed_x >= abs_period_x) {
                // Time to update X axis quadrature state
                if (x_period_us > 0) {
                    // Moving right
                    quad_mx_state++;
                    if (quad_mx_state == 4) quad_mx_state = 0;
                } else {
                    // Moving left
                    if (quad_mx_state == 0) quad_mx_state = 3;
                    else quad_mx_state--;
                }
                
                // Update GPIO based on new state
                // IMPORTANT: Only update GPIO if joystick mode is NOT active
                // When joystick mode is active, Core 0 controls these GPIOs and we must not interfere
                // Use memory barrier to ensure we see the latest joystick mode state
                __sync_synchronize();
                bool joy_mode = amiga_joystick_port1_is_joystick_mode();
                __sync_synchronize();
                if (!joy_mode) {
                    // Only update H and HQ if not in joystick mode
                    // Note: H is DOWN (GPIO 11), HQ is RIGHT (GPIO 13)
                    switch (quad_mx_state) {
                        case 0: amiga_gpio_set_active_low(QM1_AMIGA_H, false); break;   // HIGH = inactive
                        case 1: amiga_gpio_set_active_low(QM1_AMIGA_HQ, false); break;  // HIGH = inactive
                        case 2: amiga_gpio_set_active_low(QM1_AMIGA_H, true); break;    // LOW = active
                        case 3: amiga_gpio_set_active_low(QM1_AMIGA_HQ, true); break;   // LOW = active
                    }
                }
                // If joystick mode is active, we skip updating H/HQ to avoid conflicts
                
                last_x_time = current_time;
                
                // Decrement remaining motion
                if (remaining_x > 0) remaining_x--;
                else if (remaining_x < 0) remaining_x++;
                
                // If motion is complete, stop
                // Keep period constant during motion to prevent instability and "skip back" effect
                if (remaining_x == 0) {
                    x_period_us = 0;
                }
                // Don't recalculate period during motion - keep it constant for smooth, predictable movement
            }
        }
        
        // Check if it's time to update Y axis
        if (y_period_us != 0) {
            int64_t elapsed_y = absolute_time_diff_us(last_y_time, current_time);
            int32_t abs_period_y = (y_period_us < 0) ? -y_period_us : y_period_us;
            
            if (elapsed_y >= abs_period_y) {
                // Time to update Y axis quadrature state
                if (y_period_us > 0) {
                    // Moving down
                    quad_my_state++;
                    if (quad_my_state == 4) quad_my_state = 0;
                } else {
                    // Moving up
                    if (quad_my_state == 0) quad_my_state = 3;
                    else quad_my_state--;
                }
                
                // Update GPIO based on new state
                // IMPORTANT: Only update GPIO if joystick mode is NOT active
                // When joystick mode is active, Core 0 controls these GPIOs and we must not interfere
                // Use memory barrier to ensure we see the latest joystick mode state
                __sync_synchronize();
                bool joy_mode = amiga_joystick_port1_is_joystick_mode();
                __sync_synchronize();
                if (!joy_mode) {
                    // Only update V and VQ if not in joystick mode
                    // Note: V is UP (GPIO 10), VQ is LEFT (GPIO 12)
                    switch (quad_my_state) {
                        case 0: amiga_gpio_set_active_low(QM1_AMIGA_V, false); break;   // HIGH = inactive
                        case 1: amiga_gpio_set_active_low(QM1_AMIGA_VQ, false); break;  // HIGH = inactive
                        case 2: amiga_gpio_set_active_low(QM1_AMIGA_V, true); break;    // LOW = active
                        case 3: amiga_gpio_set_active_low(QM1_AMIGA_VQ, true); break;   // LOW = active
                    }
                }
                // If joystick mode is active, we skip updating V/VQ to avoid conflicts
                
                last_y_time = current_time;
                
                // Decrement remaining motion
                if (remaining_y > 0) remaining_y--;
                else if (remaining_y < 0) remaining_y++;
                
                // If motion is complete, stop
                // Keep period constant during motion to prevent instability and "skip back" effect
                if (remaining_y == 0) {
                    y_period_us = 0;
                }
                // Don't recalculate period during motion - keep it constant for smooth, predictable movement
            }
        }
        
        // Sleep briefly when idle, or wait for next update time
        if (x_period_us == 0 && y_period_us == 0) {
            // No active motion - sleep briefly and check for new input
            sleep_us(100);
        } else {
            // Active motion - calculate next update time
            int64_t next_x_time = (x_period_us != 0) ? ((x_period_us < 0) ? -x_period_us : x_period_us) - absolute_time_diff_us(last_x_time, current_time) : INT64_MAX;
            int64_t next_y_time = (y_period_us != 0) ? ((y_period_us < 0) ? -y_period_us : y_period_us) - absolute_time_diff_us(last_y_time, current_time) : INT64_MAX;
            int64_t sleep_time = (next_x_time < next_y_time) ? next_x_time : next_y_time;
            
            if (sleep_time > 0 && sleep_time < 10000) {  // Cap sleep at 10ms
                sleep_us((uint32_t)sleep_time);
            } else {
                sleep_us(100);  // Default brief sleep
            }
        }
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
