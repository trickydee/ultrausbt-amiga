/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
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
#include <hardware/gpio.h>
#include <stdio.h>

#if HIDPICO_REVISION == 5

// Helper function to set GPIO (active low)
// value=true means signal is active (LOW/0), value=false means inactive (HIGH/1)
static void _aj2_gpio_set(uint gpio, bool value)
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

void amiga_joystick_port2_init(void)
{
    // Initialize GPIO pins for Joystick Port 2
    gpio_init(QM2_AMIGA_H);
    gpio_init(QM2_AMIGA_V);
    gpio_init(QM2_AMIGA_HQ);
    gpio_init(QM2_AMIGA_VQ);
    gpio_init(QM2_AMIGA_B1);
    gpio_init(QM2_AMIGA_B2);
    gpio_init(QM2_AMIGA_B3);

    gpio_set_function(QM2_AMIGA_H, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_V, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_HQ, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_VQ, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_B1, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_B2, GPIO_FUNC_SIO);
    gpio_set_function(QM2_AMIGA_B3, GPIO_FUNC_SIO);

    // All signals are active low, so set all high (inactive) initially
    _aj2_gpio_set(QM2_AMIGA_H, false);   // No horizontal direction
    _aj2_gpio_set(QM2_AMIGA_V, false);   // No vertical direction
    _aj2_gpio_set(QM2_AMIGA_HQ, false);  // No horizontal quadrature
    _aj2_gpio_set(QM2_AMIGA_VQ, false);  // No vertical quadrature
    _aj2_gpio_set(QM2_AMIGA_B1, false);  // Fire button not pressed
    _aj2_gpio_set(QM2_AMIGA_B2, false);  // Button 2 not pressed
    _aj2_gpio_set(QM2_AMIGA_B3, false);  // Button 3 not pressed
}

// Track current direction state to avoid conflicts
static bool dir2_up = false, dir2_down = false, dir2_left = false, dir2_right = false;

void amiga_joystick_port2_set_direction(enum amiga_joystick_port2_direction dir, bool active)
{
    // Update direction state
    switch (dir) {
        case AJ2_UP:    dir2_up = active; break;
        case AJ2_DOWN:  dir2_down = active; break;
        case AJ2_LEFT:  dir2_left = active; break;
        case AJ2_RIGHT: dir2_right = active; break;
    }
    
    // Amiga joystick port 2 uses separate GPIO pins for each direction (active low):
    // Documented pinout (after swapping LEFT and DOWN back):
    // GPIO 27 (Pin 1) = UP
    // GPIO 26 (Pin 2) = DOWN  
    // GPIO 22 (Pin 3) = LEFT
    // GPIO 21 (Pin 4) = RIGHT
    // Each pin is independent: LOW = active (direction pressed), HIGH = inactive (released)
    
    // Set each direction independently using the correct GPIO pins
    // Swapped: LEFT and DOWN GPIOs (matching documented pinout)
    _aj2_gpio_set(QM2_AMIGA_V, dir2_up);      // GPIO 27 = UP
    _aj2_gpio_set(QM2_AMIGA_H, dir2_down);    // GPIO 26 = DOWN
    _aj2_gpio_set(QM2_AMIGA_VQ, dir2_left);   // GPIO 22 = LEFT
    _aj2_gpio_set(QM2_AMIGA_HQ, dir2_right);  // GPIO 21 = RIGHT
}

void amiga_joystick_port2_set_button(enum amiga_joystick_port2_buttons button, bool pressed)
{
    switch (button) {
        case AJ2_FIRE:
            _aj2_gpio_set(QM2_AMIGA_B1, pressed);
            break;
        case AJ2_BUTTON2:
            _aj2_gpio_set(QM2_AMIGA_B2, pressed);
            break;
        case AJ2_BUTTON3:
            _aj2_gpio_set(QM2_AMIGA_B3, pressed);
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
#if HIDPICO_REVISION == 5
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
    _aj2_gpio_set(QM2_AMIGA_H, false);
    _aj2_gpio_set(QM2_AMIGA_V, false);
    _aj2_gpio_set(QM2_AMIGA_HQ, false);
    _aj2_gpio_set(QM2_AMIGA_VQ, false);
    _aj2_gpio_set(QM2_AMIGA_B1, false);
    _aj2_gpio_set(QM2_AMIGA_B2, false);
    _aj2_gpio_set(QM2_AMIGA_B3, false);
}

#else
// Joystick Port 2 is only available on Revision 5
void amiga_joystick_port2_init(void) {}
void amiga_joystick_port2_set_direction(enum amiga_joystick_port2_direction dir, bool active) { (void)dir; (void)active; }
void amiga_joystick_port2_set_button(enum amiga_joystick_port2_buttons button, bool pressed) { (void)button; (void)pressed; }
void amiga_joystick_port2_reset(void) {}
#endif // HIDPICO_REVISION == 5
