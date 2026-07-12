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
#include "platform/amiga/port_mode.h"
#include <stdio.h>
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
// Core 0 diagnostics: non-zero motion samples fed into Core 1
volatile uint32_t g_mouse_motion_feed_count = 0;
volatile int8_t g_mouse_last_dx = 0;
volatile int8_t g_mouse_last_dy = 0;

// Core 1 pause: BT enumeration (refcount) + Port 1 CD32 (single flag)
volatile bool g_core1_paused = false;
static volatile uint32_t g_bt_pause_depth = 0;
static volatile bool g_cd32_pause = false;

enum {
    CORE1_PHASE_LOOP_TOP = 0,
    CORE1_PHASE_PAUSED = 1,
    CORE1_PHASE_MOTION = 2,
};
static volatile uint32_t g_core1_phase = CORE1_PHASE_LOOP_TOP;
static volatile uint32_t g_core1_pause_spins = 0;

// Core 1 heartbeat counter - increments every loop to detect if Core 1 is running
volatile uint32_t g_core1_heartbeat = 0;

static absolute_time_t g_bt_pause_watchdog_start;
static bool g_bt_pause_watchdog_active = false;

static void core1_recompute_paused(void)
{
    g_core1_paused = (g_bt_pause_depth > 0) || g_cd32_pause;
}

static void core1_bt_pause_watchdog_arm(void)
{
    if (g_bt_pause_depth == 1) {
        g_bt_pause_watchdog_start = get_absolute_time();
        g_bt_pause_watchdog_active = true;
    }
}

static void core1_bt_pause_watchdog_disarm(void)
{
    if (g_bt_pause_depth == 0) {
        g_bt_pause_watchdog_active = false;
    }
}

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
#if HIDPICO_REV_ATARI_BOARD
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
        g_mouse_last_dx = scaled_x;
        g_mouse_last_dy = scaled_y;
        g_mouse_motion_feed_count++;

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
        g_core1_heartbeat++;
        g_core1_phase = CORE1_PHASE_LOOP_TOP;

        __sync_synchronize();
        bool paused = g_core1_paused;
        __sync_synchronize();

        if (paused) {
            g_core1_phase = CORE1_PHASE_PAUSED;
            g_core1_pause_spins++;
            // Avoid bare __wfe(): after BT flash lockout it can miss SEV and park forever.
            busy_wait_us(100);
            continue;
        }

        g_core1_phase = CORE1_PHASE_MOTION;
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
#if HIDPICO_REV_ATARI_BOARD
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
#if HIDPICO_REV_ATARI_BOARD
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
        
        // busy_wait only — sleep_us can hang on Core 1 after BT flash/bond activity
        busy_wait_us(50);
    }
}

// Port 1 CD32 — pause quadrature GPIO updates on Core 1
void amiga_quad_mouse_pause_core1(void)
{
    __sync_synchronize();
    g_cd32_pause = true;
    core1_recompute_paused();
    __sync_synchronize();
    __dmb();
}

void amiga_quad_mouse_resume_core1(void)
{
    __sync_synchronize();
    g_cd32_pause = false;
    core1_recompute_paused();
    __sync_synchronize();
    __dmb();
}

static void core1_wake_from_pause(void)
{
    // Core 1 waits in __wfe(); clear the pause flag then SEV so it leaves WFE.
    // Double SEV covers the case where Core 1 was between the paused check and WFE.
    __sev();
    __sev();
}

void core1_pause_for_bt_enumeration(void)
{
    // Do not stack pauses: a failed Stadia reconnect can rediscover without
    // calling on_device_disconnected, and a second ++depth leaves Core 1 stuck
    // after a single resume-by-1 on device ready.
    if (g_bt_pause_depth > 0) {
        return;
    }
    g_bt_pause_depth = 1;
    g_core1_pause_spins = 0;
    __dmb();
    core1_recompute_paused();
    __dmb();
    core1_bt_pause_watchdog_arm();
}

void core1_wait_for_pause_active(uint32_t timeout_ms)
{
    uint32_t limit = timeout_ms * 100u;
    for (uint32_t i = 0; i < limit; i++) {
        if (g_core1_pause_spins > 0 || g_core1_phase == CORE1_PHASE_PAUSED) {
            return;
        }
        busy_wait_us(10);
    }
}

void core1_resume_after_bt_enumeration(void)
{
    // Prefer force-release semantics for BT enumeration: a single successful
    // ready/disconnect must clear any orphaned discovery pause.
    core1_force_release_bt_pause();
}

uint32_t core1_get_bt_pause_depth(void)
{
    return g_bt_pause_depth;
}

void core1_force_release_bt_pause(void)
{
    if (g_bt_pause_depth == 0) {
        // Still poke Core 1 in case it is sitting in WFE with a stale view.
        core1_wake_from_pause();
        return;
    }
    g_bt_pause_depth = 0;
    __dmb();
    core1_recompute_paused();
    __dmb();
    core1_bt_pause_watchdog_disarm();
    core1_wake_from_pause();
}

bool core1_bt_pause_watchdog_tick(void)
{
    if (!g_bt_pause_watchdog_active || g_bt_pause_depth == 0) {
        return false;
    }

    int64_t elapsed_us = absolute_time_diff_us(g_bt_pause_watchdog_start, get_absolute_time());
    if (elapsed_us < (int64_t)BT_CORE1_PAUSE_WATCHDOG_MS * 1000) {
        return false;
    }

    printf("[BT] Core 1 BT pause watchdog: forcing resume (depth was %lu)\n",
           (unsigned long)g_bt_pause_depth);
    core1_force_release_bt_pause();
    return true;
}

bool core1_heartbeat_watchdog_tick(void)
{
    static uint32_t last_hb;
    static absolute_time_t last_change;
    static absolute_time_t last_diag;
    static bool armed;
    static bool relaunched;

    uint32_t hb = g_core1_heartbeat;
    absolute_time_t now = get_absolute_time();

    if (!armed) {
        last_hb = hb;
        last_change = now;
        last_diag = now;
        armed = true;
        return false;
    }

    // Periodic visibility while debugging Stadia mouse lockups
    if (absolute_time_diff_us(last_diag, now) >= 2000000) {
        printf("[DIAG] Core1 hb=%lu phase=%lu paused=%d bt_depth=%lu cd32_pause=%d joy=%d port1=%s motion_feeds=%lu last_d=(%d,%d) flag=%d\n",
               (unsigned long)hb,
               (unsigned long)g_core1_phase,
               g_core1_paused ? 1 : 0,
               (unsigned long)g_bt_pause_depth,
               g_cd32_pause ? 1 : 0,
               amiga_joystick_port1_is_joystick_mode() ? 1 : 0,
               port_mode_port1_label(),
               (unsigned long)g_mouse_motion_feed_count,
               (int)g_mouse_last_dx,
               (int)g_mouse_last_dy,
               motion_flag ? 1 : 0);
        last_diag = now;
    }

    if (hb != last_hb) {
        last_hb = hb;
        last_change = now;
        relaunched = false;
        return false;
    }

    // Heartbeat frozen — only act if we are not intentionally paused
    if (g_bt_pause_depth > 0 || g_cd32_pause) {
        last_change = now;
        return false;
    }

    int64_t stalled_us = absolute_time_diff_us(last_change, now);
    if (stalled_us < (int64_t)CORE1_HEARTBEAT_STALL_MS * 1000) {
        return false;
    }

    printf("[BT] Core 1 heartbeat stalled (hb=%lu phase=%lu) — SEV wake\n",
           (unsigned long)hb, (unsigned long)g_core1_phase);
    core1_force_release_bt_pause();
    last_change = now;

    if (!relaunched) {
        busy_wait_us(2000);
        if (g_core1_heartbeat == hb) {
            printf("[BT] Core 1 still stalled — relaunching amiga_quad_mouse_motion\n");
            multicore_reset_core1();
            busy_wait_us(1000);
            multicore_launch_core1(amiga_quad_mouse_motion);
            relaunched = true;
            last_hb = g_core1_heartbeat;
            last_change = get_absolute_time();
            return true;
        }
    }
    return true;
}
