/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse mouse path — decode an Amiga quadrature mouse plugged into Port 1.
 *
 * Each axis has two signals 90 degrees out of phase (H/HQ, V/VQ). We poll the
 * lines, feed a standard 2-bit gray-code quadrature decoder and accumulate signed
 * movement, then emit a relative USB mouse report at a fixed cadence. Buttons are
 * active-low on fire / B2 / B3.
 *
 * Note: axis sign and left/right/middle button assignment follow the Port 1 mouse
 * wiring; if motion is mirrored on real hardware, flip the sign in the report step.
 */

#include "config.h"

#if ENABLE_USB_DEVICE_MODE

#include "mouse_host_in.h"
#include "usb_hid_device.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "class/hid/hid.h"
#include <stdio.h>

// Port 1 quadrature + button GPIOs (from config.h Rev 5/6 map).
#define MHI_X_A   QM1_AMIGA_H    // horizontal
#define MHI_X_B   QM1_AMIGA_HQ   // horizontal quadrature
#define MHI_Y_A   QM1_AMIGA_V    // vertical
#define MHI_Y_B   QM1_AMIGA_VQ   // vertical quadrature
#define MHI_BTN_L QM1_AMIGA_B1   // fire  -> left
#define MHI_BTN_R QM1_AMIGA_B2   // pin 9 -> right
#define MHI_BTN_M QM1_AMIGA_B3   // pin 5 -> middle

#define MHI_REPORT_INTERVAL_US 8000

// Quadrature transition table: index = (prev_state << 2) | new_state, each state
// is (A << 1) | B. Valid single-step transitions yield +/-1, others 0.
static const int8_t s_qdec[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0,
};

static uint8_t s_prev_x = 0, s_prev_y = 0;
static int32_t s_accum_x = 0, s_accum_y = 0;
static uint8_t s_prev_buttons = 0xff;  // force first report
static uint32_t s_last_report_us = 0;

static inline uint8_t read_state(uint gpio_a, uint gpio_b)
{
    return (uint8_t)((gpio_get(gpio_a) << 1) | gpio_get(gpio_b));
}

static void setup_input(uint gpio)
{
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

void mouse_host_in_init(void)
{
    setup_input(MHI_X_A);
    setup_input(MHI_X_B);
    setup_input(MHI_Y_A);
    setup_input(MHI_Y_B);
    setup_input(MHI_BTN_L);
    setup_input(MHI_BTN_R);
    setup_input(MHI_BTN_M);

    s_prev_x = read_state(MHI_X_A, MHI_X_B);
    s_prev_y = read_state(MHI_Y_A, MHI_Y_B);
    s_accum_x = s_accum_y = 0;
    s_prev_buttons = 0xff;
    s_last_report_us = time_us_32();

    printf("[mouse-in] Amiga Port 1 quadrature mouse read active\n");
}

static int8_t clamp_i8(int32_t v)
{
    if (v > 127) return 127;
    if (v < -127) return -127;
    return (int8_t)v;
}

void mouse_host_in_task(void)
{
    // Poll quadrature on every call so fast motion is not missed.
    uint8_t xs = read_state(MHI_X_A, MHI_X_B);
    uint8_t ys = read_state(MHI_Y_A, MHI_Y_B);
    s_accum_x += s_qdec[(s_prev_x << 2) | xs];
    s_accum_y += s_qdec[(s_prev_y << 2) | ys];
    s_prev_x = xs;
    s_prev_y = ys;

    // Buttons are active-low (pressed = 0).
    uint8_t buttons = 0;
    if (!gpio_get(MHI_BTN_L)) buttons |= MOUSE_BUTTON_LEFT;
    if (!gpio_get(MHI_BTN_R)) buttons |= MOUSE_BUTTON_RIGHT;
    if (!gpio_get(MHI_BTN_M)) buttons |= MOUSE_BUTTON_MIDDLE;

    uint32_t now = time_us_32();
    bool due = (now - s_last_report_us) >= MHI_REPORT_INTERVAL_US;
    bool motion = (s_accum_x != 0 || s_accum_y != 0);
    bool btn_change = (buttons != s_prev_buttons);

    if ((due && motion) || btn_change) {
        int8_t dx = clamp_i8(s_accum_x);
        int8_t dy = clamp_i8(s_accum_y);
        s_accum_x -= dx;
        s_accum_y -= dy;
        s_prev_buttons = buttons;
        s_last_report_us = now;
        usb_hid_device_send_mouse(buttons, dx, dy, 0);
    }
}

#endif // ENABLE_USB_DEVICE_MODE
