/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * https://github.com/borb/amigahid-pico
 *
 * Modifications Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga joystick port 2 interface implementation.
 *
 * Joystick Port 2 uses dedicated GPIO pins (QM2_AMIGA_*).
 * This provides simple digital joystick emulation by controlling direction
 * and button signals directly.
 */

#include "joystick_port2.h"
#include "config.h"
#include "platform/common/gpio_util.h"
#include "cd32_pad.h"
#include "util/output.h"
#include <hardware/gpio.h>

#if HIDPICO_REV_ATARI_BOARD

void amiga_joystick_port2_init(void)
{
    // Initialize GPIO pins for Joystick Port 2 using optimized utility
    // All signals are active low, so set all high (inactive) initially
    amiga_gpio_init_active_low(QM2_AMIGA_H, false);   // No horizontal direction
    amiga_gpio_init_active_low(QM2_AMIGA_V, false);   // No vertical direction
    amiga_gpio_init_active_low(QM2_AMIGA_HQ, false);  // No horizontal quadrature
    amiga_gpio_init_active_low(QM2_AMIGA_VQ, false);  // No vertical quadrature
    amiga_gpio_init_active_low(QM2_AMIGA_B1, false);  // Fire button not pressed
    amiga_gpio_init_active_low(QM2_AMIGA_B2, false);  // Button 2 not pressed
    amiga_gpio_init_active_low(QM2_AMIGA_B3, false);  // Button 3 not pressed
}

// Track current direction state to avoid conflicts and optimize GPIO updates
static bool dir2_up = false, dir2_down = false, dir2_left = false, dir2_right = false;
// Track previous GPIO states to only update changed pins (optimization)
static bool prev_dir2_up = false, prev_dir2_down = false, prev_dir2_left = false, prev_dir2_right = false;

void amiga_joystick_port2_set_direction(enum amiga_joystick_port2_direction dir, bool active)
{
    // Update direction state
    switch (dir) {
        case AJ2_UP:    dir2_up = active; break;
        case AJ2_DOWN:  dir2_down = active; break;
        case AJ2_LEFT:  dir2_left = active; break;
        case AJ2_RIGHT: dir2_right = active; break;
    }

    if (cd32_port2_is_enabled()) {
        uint8_t bits = (dir2_up ? 0x01 : 0) | (dir2_down ? 0x02 : 0) |
                       (dir2_left ? 0x04 : 0) | (dir2_right ? 0x08 : 0);
        cd32_port2_update_dpad(bits);
        return;
    }
    
    // Amiga joystick port 2 uses separate GPIO pins for each direction (active low):
    // Documented pinout (after swapping LEFT and DOWN back):
    // GPIO 19 (QM2_AMIGA_V) = UP
    // GPIO 20 (QM2_AMIGA_H) = DOWN  
    // GPIO 21 (QM2_AMIGA_VQ) = LEFT
    // GPIO 22 (QM2_AMIGA_HQ) = RIGHT
    // Each pin is independent: LOW = active (direction pressed), HIGH = inactive (released)
    
    // Optimized: Only update GPIO pins that have changed state
    // This reduces GPIO operations by ~75% in typical usage (only 1 direction changes at a time)
    if (dir2_up != prev_dir2_up) {
        amiga_gpio_set_active_low(QM2_AMIGA_V, dir2_up);      // GPIO 19 = UP
        prev_dir2_up = dir2_up;
    }
    if (dir2_down != prev_dir2_down) {
        amiga_gpio_set_active_low(QM2_AMIGA_H, dir2_down);    // GPIO 20 = DOWN
        prev_dir2_down = dir2_down;
    }
    if (dir2_left != prev_dir2_left) {
        amiga_gpio_set_active_low(QM2_AMIGA_VQ, dir2_left);   // GPIO 21 = LEFT
        prev_dir2_left = dir2_left;
    }
    if (dir2_right != prev_dir2_right) {
        amiga_gpio_set_active_low(QM2_AMIGA_HQ, dir2_right);  // GPIO 22 = RIGHT
        prev_dir2_right = dir2_right;
    }
}

// Track previous button states to only update changed buttons (optimization)
static bool prev_button1 = false, prev_button2 = false, prev_button3 = false;

void amiga_joystick_port2_set_button(enum amiga_joystick_port2_buttons button, bool pressed)
{
    if (cd32_port2_is_enabled()) {
        cd32_port2_legacy_button(button, pressed);
        return;
    }

    // Optimized: Only update GPIO if button state has changed
    switch (button) {
        case AJ2_FIRE:
            if (pressed != prev_button1) {
                amiga_gpio_set_active_low(QM2_AMIGA_B1, pressed);
                prev_button1 = pressed;
            }
            break;
        case AJ2_BUTTON2:
            if (pressed != prev_button2) {
                amiga_gpio_set_active_low(QM2_AMIGA_B2, pressed);
                prev_button2 = pressed;
            }
            break;
        case AJ2_BUTTON3:
            // Button 3 — GPIO from QM2_AMIGA_B3 (Rev 5: 28, Rev 6: 1)
            if (pressed != prev_button3) {
                amiga_gpio_set_active_low(QM2_AMIGA_B3, pressed);
                prev_button3 = pressed;
            }
            break;
    }
}

// Getter functions for debugging
bool amiga_joystick_port2_get_direction(enum amiga_joystick_port2_direction dir)
{
    switch (dir) {
        case AJ2_UP:    return dir2_up;
        case AJ2_DOWN:  return dir2_down;
        case AJ2_LEFT:  return dir2_left;
        case AJ2_RIGHT: return dir2_right;
    }
    return false;
}

bool amiga_joystick_port2_get_button(enum amiga_joystick_port2_buttons button)
{
    // Note: We don't track button state internally, so we read from GPIO
    // This is a simple implementation - buttons are active low
#if HIDPICO_REV_ATARI_BOARD
    switch (button) {
        case AJ2_FIRE:
            return !gpio_get(QM2_AMIGA_B1);  // Active low, so invert
        case AJ2_BUTTON2:
            return !gpio_get(QM2_AMIGA_B2);
        case AJ2_BUTTON3:
            return !gpio_get(QM2_AMIGA_B3);
    }
#endif
    return false;
}

void amiga_joystick_port2_reset(void)
{
    // Reset all directions and buttons
    dir2_up = dir2_down = dir2_left = dir2_right = false;
    prev_dir2_up = prev_dir2_down = prev_dir2_left = prev_dir2_right = false;
    prev_button1 = prev_button2 = prev_button3 = false;
    
    // Use optimized GPIO utility - will only update if state actually changes
    amiga_gpio_set_active_low(QM2_AMIGA_H, false);
    amiga_gpio_set_active_low(QM2_AMIGA_V, false);
    amiga_gpio_set_active_low(QM2_AMIGA_HQ, false);
    amiga_gpio_set_active_low(QM2_AMIGA_VQ, false);
    amiga_gpio_set_active_low(QM2_AMIGA_B1, false);
    amiga_gpio_set_active_low(QM2_AMIGA_B2, false);
    amiga_gpio_set_active_low(QM2_AMIGA_B3, false);
}

#else
// Joystick Port 2 is only available on Revision 5
void amiga_joystick_port2_init(void) {}
void amiga_joystick_port2_set_direction(enum amiga_joystick_port2_direction dir, bool active) { (void)dir; (void)active; }
void amiga_joystick_port2_set_button(enum amiga_joystick_port2_buttons button, bool pressed) { (void)button; (void)pressed; }
void amiga_joystick_port2_reset(void) {}
#endif // HIDPICO_REV_ATARI_BOARD
