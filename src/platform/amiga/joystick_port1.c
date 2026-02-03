/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga joystick port 1 interface implementation.
 * 
 * Joystick Port 1 uses the same GPIO pins as the mouse (QM1_AMIGA_*).
 * This provides simple digital joystick emulation by controlling direction
 * and button signals directly.
 * 
 * NOTE: Mouse quadrature encoding (running on Core 1) and joystick signals
 * (running on Core 0) both control the same GPIO pins. They will conflict
 * if both are active simultaneously. The joystick signals will override
 * the mouse quadrature signals when active.
 */

#include "joystick_port1.h"
#include "config.h"
#include "platform/common/gpio_util.h"
#include <hardware/gpio.h>
#include "hardware/sync.h"  // For memory barriers (__sync_synchronize)

// Movement threshold for mouse-to-joystick conversion (in HID report units)
// Mouse movement must exceed this threshold to trigger joystick direction
#define MOUSE_TO_JOYSTICK_THRESHOLD 5

// Port 1 mode: false = mouse only (default), true = joystick mode (mouse converted to joystick)
// Volatile to ensure Core 1 (mouse quadrature) sees updates from Core 0
static volatile bool port1_joystick_mode = false;

void amiga_joystick_port1_init(void)
{
    // Initialize GPIO pins using optimized shared utility
    // All signals are active low, so set all high (inactive) initially
    amiga_gpio_init_active_low(QM1_AMIGA_H, false);   // No horizontal direction
    amiga_gpio_init_active_low(QM1_AMIGA_V, false);   // No vertical direction
    amiga_gpio_init_active_low(QM1_AMIGA_HQ, false);  // No horizontal quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_VQ, false);  // No vertical quadrature
    amiga_gpio_init_active_low(QM1_AMIGA_B1, false);  // Fire button not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B2, false);  // Button 2 not pressed
    amiga_gpio_init_active_low(QM1_AMIGA_B3, false);  // Button 3 not pressed
}

// Track current direction state to avoid conflicts and optimize GPIO updates
static bool dir_up = false, dir_down = false, dir_left = false, dir_right = false;
// Track previous GPIO states to only update changed pins (optimization - matches port 2)
static bool prev_dir_up = false, prev_dir_down = false, prev_dir_left = false, prev_dir_right = false;

void amiga_joystick_port1_set_direction(enum amiga_joystick_port1_direction dir, bool active)
{
    // Update direction state
    switch (dir) {
        case AJ1_UP:    dir_up = active; break;
        case AJ1_DOWN:  dir_down = active; break;
        case AJ1_LEFT:  dir_left = active; break;
        case AJ1_RIGHT: dir_right = active; break;
    }
    
    // Amiga joystick port 1: Use separate pins for each direction (same approach as port 2)
    // H pin (GPIO 11) = DOWN direction (LOW = active)
    // HQ pin (GPIO 13) = RIGHT direction (LOW = active)  
    // V pin (GPIO 10) = UP direction (LOW = active)
    // VQ pin (GPIO 12) = LEFT direction (LOW = active)
    // All signals are active low, so LOW = direction pressed, HIGH/inactive = released
    
    // Optimized: Only update GPIO pins that have changed state (matches port 2 optimization)
    // This reduces GPIO operations by ~75% in typical usage (only 1 direction changes at a time)
    if (dir_up != prev_dir_up) {
        amiga_gpio_set_active_low(QM1_AMIGA_V, dir_up);     // V pin = UP
        prev_dir_up = dir_up;
    }
    if (dir_down != prev_dir_down) {
        amiga_gpio_set_active_low(QM1_AMIGA_H, dir_down);   // H pin = DOWN
        prev_dir_down = dir_down;
    }
    if (dir_left != prev_dir_left) {
        // Use memory barrier before setting GPIO to ensure Core 1 sees joystick mode change
        __sync_synchronize();
        amiga_gpio_set_active_low(QM1_AMIGA_VQ, dir_left);  // VQ pin = LEFT (GPIO 12)
        __sync_synchronize();
        prev_dir_left = dir_left;
    }
    if (dir_right != prev_dir_right) {
        // Use memory barrier before setting GPIO to ensure Core 1 sees joystick mode change
        __sync_synchronize();
        amiga_gpio_set_active_low(QM1_AMIGA_HQ, dir_right); // HQ pin = RIGHT (GPIO 13)
        __sync_synchronize();
        prev_dir_right = dir_right;
    }
}

void amiga_joystick_port1_set_button(enum amiga_joystick_port1_buttons button, bool pressed)
{
    switch (button) {
        case AJ1_FIRE:
            amiga_gpio_set_active_low(QM1_AMIGA_B1, pressed);
            break;
        case AJ1_BUTTON2:
            // Button 2 now uses GPIO 2 (remapped from GPIO 12) - no conflicts
            amiga_gpio_set_active_low(QM1_AMIGA_B2, pressed);
            break;
        case AJ1_BUTTON3:
            // Button 3 now uses GPIO 3 (remapped from GPIO 13) - no conflicts
            amiga_gpio_set_active_low(QM1_AMIGA_B3, pressed);
            break;
    }
}

void amiga_joystick_port1_set_from_mouse(int8_t x, int8_t y, uint8_t buttons)
{
    // Convert mouse movement to joystick directions
    // Use threshold to avoid jitter from small movements
    
    // Horizontal direction
    if (x > MOUSE_TO_JOYSTICK_THRESHOLD) {
        // Moving right
        amiga_joystick_port1_set_direction(AJ1_LEFT, false);
        amiga_joystick_port1_set_direction(AJ1_RIGHT, true);
    } else if (x < -MOUSE_TO_JOYSTICK_THRESHOLD) {
        // Moving left
        amiga_joystick_port1_set_direction(AJ1_RIGHT, false);
        amiga_joystick_port1_set_direction(AJ1_LEFT, true);
    } else {
        // No horizontal movement
        amiga_joystick_port1_set_direction(AJ1_LEFT, false);
        amiga_joystick_port1_set_direction(AJ1_RIGHT, false);
    }
    
    // Vertical direction
    if (y > MOUSE_TO_JOYSTICK_THRESHOLD) {
        // Moving down
        amiga_joystick_port1_set_direction(AJ1_UP, false);
        amiga_joystick_port1_set_direction(AJ1_DOWN, true);
    } else if (y < -MOUSE_TO_JOYSTICK_THRESHOLD) {
        // Moving up
        amiga_joystick_port1_set_direction(AJ1_DOWN, false);
        amiga_joystick_port1_set_direction(AJ1_UP, true);
    } else {
        // No vertical movement
        amiga_joystick_port1_set_direction(AJ1_UP, false);
        amiga_joystick_port1_set_direction(AJ1_DOWN, false);
    }
    
    // Map mouse buttons to joystick buttons
    // Left button -> Fire
    // Right button -> Button 2
    // Middle button -> Button 3
    amiga_joystick_port1_set_button(AJ1_FIRE, buttons & 0x01);      // Left button
    amiga_joystick_port1_set_button(AJ1_BUTTON2, buttons & 0x02);  // Right button
    amiga_joystick_port1_set_button(AJ1_BUTTON3, buttons & 0x04);  // Middle button
}

void amiga_joystick_port1_toggle_mode(void)
{
    // Use memory barrier to ensure Core 1 sees the mode change
    __sync_synchronize();
    port1_joystick_mode = !port1_joystick_mode;
    __sync_synchronize();
    
    // When switching to mouse-only mode, release all joystick signals
    if (!port1_joystick_mode) {
        amiga_joystick_port1_set_direction(AJ1_UP, false);
        amiga_joystick_port1_set_direction(AJ1_DOWN, false);
        amiga_joystick_port1_set_direction(AJ1_LEFT, false);
        amiga_joystick_port1_set_direction(AJ1_RIGHT, false);
        amiga_joystick_port1_set_button(AJ1_FIRE, false);
        amiga_joystick_port1_set_button(AJ1_BUTTON2, false);
        amiga_joystick_port1_set_button(AJ1_BUTTON3, false);
    }
}

bool amiga_joystick_port1_is_joystick_mode(void)
{
    // Use memory barrier to ensure we read the latest value from Core 0
    __sync_synchronize();
    bool mode = port1_joystick_mode;
    __sync_synchronize();
    return mode;
}

