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
#include "util/output.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

// mouse motion values, used between core0 and core1
volatile int8_t x = 0, y = 0;
volatile bool motion_flag = false;

// Core 1 pause flag - set to true to pause mouse processing (e.g., during Bluetooth enumeration)
volatile bool g_core1_paused = false;

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
    // Store motion directly - processing loop checks frequently for smooth movement
    // For race conditions: if values are being processed, add to existing values
    // This allows small movements to be processed immediately without accumulation delay
    if (in_x != 0 || in_y != 0) {
        x = in_x;
        y = in_y;
        motion_flag = true;
    }

    // @todo use fifo write here to unblock core1 thread?
}

void amiga_quad_mouse_motion()
{
    // ahprintf("[aqm] hello from core1, mouse motion output loop starting\n");
    int8_t out_x, out_y;
    uint8_t quad_mx_state = 0, quad_my_state = 0;

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
     */

    while (1) {
        // Check if Core 1 is paused (e.g., during Bluetooth enumeration)
        // This prevents flash access conflicts during GATT service discovery
        if (g_core1_paused) {
            sleep_ms(10);  // Sleep longer when paused
            continue;
        }
        
        // Check for new motion frequently to ensure smooth processing of slow movements
        // This prevents accumulation and jerky behavior
        if (!motion_flag) {
            // No new motion - sleep briefly and check again
            sleep_us(100);
            continue;
        }

        // Read motion atomically
        out_x = x;
        out_y = y;
        x = y = 0;
        motion_flag = false;

        // Process all motion immediately - no accumulation delay
        while ((out_x != 0) || (out_y != 0)) {
            // Process all x-axis motion (removed divider skip logic for better sensitivity)
            if (out_x != 0) {
                // handle x-axis motion
                if (out_x < 0)
                    quad_mx_state--;
                else if (out_x > 0)
                    quad_mx_state++;
                // fix wraparound
                if (quad_mx_state == 255)
                    quad_mx_state = 3;
                else if (quad_mx_state == 4)
                    quad_mx_state = 0;

                switch (quad_mx_state) {
                    case 0: amiga_gpio_set_active_low(QM1_AMIGA_H, false); break;   // HIGH = inactive
                    case 1: amiga_gpio_set_active_low(QM1_AMIGA_HQ, false); break;  // HIGH = inactive
                    case 2: amiga_gpio_set_active_low(QM1_AMIGA_H, true); break;    // LOW = active
                    case 3: amiga_gpio_set_active_low(QM1_AMIGA_HQ, true); break;   // LOW = active
                }
            }

            if (out_x < 0) out_x++;
            if (out_x > 0) out_x--;

            // Process all y-axis motion (removed divider skip logic for better sensitivity)
            if (out_y != 0) {
                // handle y-axis motion
                if (out_y < 0)
                    quad_my_state--;
                else if (out_y > 0)
                    quad_my_state++;
                // fix wraparound
                if (quad_my_state == 255)
                    quad_my_state = 3;
                else if (quad_my_state == 4)
                    quad_my_state = 0;

                switch (quad_my_state) {
                    case 0: amiga_gpio_set_active_low(QM1_AMIGA_V, false); break;   // HIGH = inactive
                    case 1: amiga_gpio_set_active_low(QM1_AMIGA_VQ, false); break;  // HIGH = inactive
                    case 2: amiga_gpio_set_active_low(QM1_AMIGA_V, true); break;    // LOW = active
                    case 3: amiga_gpio_set_active_low(QM1_AMIGA_VQ, true); break;   // LOW = active
                }
            }

            if (out_y < 0) out_y++;
            if (out_y > 0) out_y--;

            sleep_us(300); // delay before next iteration to prevent missing state change
        }
    }
}

// Core 1 pause/resume functions for Bluetooth enumeration coordination
void amiga_quad_mouse_pause_core1(void)
{
    g_core1_paused = true;
}

void amiga_quad_mouse_resume_core1(void)
{
    g_core1_paused = false;
}
