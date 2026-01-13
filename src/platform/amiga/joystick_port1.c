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
#include <hardware/gpio.h>

// Movement threshold for mouse-to-joystick conversion (in HID report units)
// Mouse movement must exceed this threshold to trigger joystick direction
#define MOUSE_TO_JOYSTICK_THRESHOLD 5

// Helper function to set GPIO (active low)
// value=true means signal is active (LOW/0), value=false means inactive (HIGH/1)
static void _aj1_gpio_set(uint gpio, bool value)
{
    if (value) {
        // Active: set to LOW (0)
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
    } else {
        // Inactive: set to HIGH (1) by setting as input (pulled high)
        gpio_set_dir(gpio, GPIO_IN);
    }
}

void amiga_joystick_port1_init(void)
{
    // Initialize GPIO pins (same as mouse interface)
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

    // All signals are active low, so set all high (inactive) initially
    _aj1_gpio_set(QM1_AMIGA_H, false);   // No horizontal direction
    _aj1_gpio_set(QM1_AMIGA_V, false);   // No vertical direction
    _aj1_gpio_set(QM1_AMIGA_HQ, false);  // No horizontal quadrature
    _aj1_gpio_set(QM1_AMIGA_VQ, false);  // No vertical quadrature
    _aj1_gpio_set(QM1_AMIGA_B1, false);  // Fire button not pressed
    _aj1_gpio_set(QM1_AMIGA_B2, false);  // Button 2 not pressed
    _aj1_gpio_set(QM1_AMIGA_B3, false);  // Button 3 not pressed
}

// Track current direction state to avoid conflicts
static bool dir_up = false, dir_down = false, dir_left = false, dir_right = false;

void amiga_joystick_port1_set_direction(enum amiga_joystick_port1_direction dir, bool active)
{
    // Update direction state
    switch (dir) {
        case AJ1_UP:    dir_up = active; break;
        case AJ1_DOWN:  dir_down = active; break;
        case AJ1_LEFT:  dir_left = active; break;
        case AJ1_RIGHT: dir_right = active; break;
    }
    
    // Amiga joystick port uses simple digital signals (active low):
    // H pin: LOW = left, HIGH = right (or no horizontal)
    // V pin: LOW = up, HIGH = down (or no vertical)
    // For joystick mode, we don't use quadrature signals (HQ/VQ)
    
    // Set horizontal direction
    if (dir_left && !dir_right) {
        // Left only
        _aj1_gpio_set(QM1_AMIGA_H, true);   // LOW = active (left)
    } else if (dir_right && !dir_left) {
        // Right only
        _aj1_gpio_set(QM1_AMIGA_H, false);  // HIGH = inactive (right)
    } else {
        // No horizontal or conflicting directions
        _aj1_gpio_set(QM1_AMIGA_H, false);  // HIGH = inactive (no direction)
    }
    
    // Set vertical direction
    if (dir_up && !dir_down) {
        // Up only
        _aj1_gpio_set(QM1_AMIGA_V, true);   // LOW = active (up)
    } else if (dir_down && !dir_up) {
        // Down only
        _aj1_gpio_set(QM1_AMIGA_V, false);  // HIGH = inactive (down)
    } else {
        // No vertical or conflicting directions
        _aj1_gpio_set(QM1_AMIGA_V, false);  // HIGH = inactive (no direction)
    }
    
    // Quadrature signals not used for joystick mode, keep them inactive
    _aj1_gpio_set(QM1_AMIGA_HQ, false);
    _aj1_gpio_set(QM1_AMIGA_VQ, false);
}

void amiga_joystick_port1_set_button(enum amiga_joystick_port1_buttons button, bool pressed)
{
    switch (button) {
        case AJ1_FIRE:
            _aj1_gpio_set(QM1_AMIGA_B1, pressed);
            break;
        case AJ1_BUTTON2:
            _aj1_gpio_set(QM1_AMIGA_B2, pressed);
            break;
        case AJ1_BUTTON3:
            _aj1_gpio_set(QM1_AMIGA_B3, pressed);
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

