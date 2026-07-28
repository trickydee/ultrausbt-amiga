/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse Port 2 path — read an Atari-style digital joystick on Port 2.
 *
 * Classic DE-9 (Amiga/Atari pinout): pins 1–4 directions, pin 6 fire, pin 9
 * button 2 — all active-low to GND. Pin 5 (button 3) is ignored for the
 * two-button profile. Straight cable into Port 2; no remapper required.
 */

#include "config.h"

#if ENABLE_USB_DEVICE_MODE && HIDPICO_REV_ATARI_BOARD

#include "joystick_port2_host_in.h"
#include "usb_hid_device.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "class/hid/hid.h"
#include <stdio.h>

#define J2HI_UP    QM2_AMIGA_V
#define J2HI_DOWN  QM2_AMIGA_H
#define J2HI_LEFT  QM2_AMIGA_VQ
#define J2HI_RIGHT QM2_AMIGA_HQ
#define J2HI_FIRE  QM2_AMIGA_B1
#define J2HI_B2    QM2_AMIGA_B2

#define J2HI_REPORT_INTERVAL_US 8000

static uint8_t s_prev_hat = 0xff;
static uint32_t s_prev_buttons = 0xffffffffu;
static uint32_t s_last_report_us = 0;

static void setup_input(uint gpio)
{
    gpio_init(gpio);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

static inline bool pressed(uint gpio)
{
    return !gpio_get(gpio);  // active low
}

static void cancel_opposites(bool *a, bool *b)
{
    if (*a && *b) {
        *a = *b = false;
    }
}

static uint8_t dirs_to_hat(bool up, bool down, bool left, bool right)
{
    if (up && right)  return GAMEPAD_HAT_UP_RIGHT;
    if (down && right) return GAMEPAD_HAT_DOWN_RIGHT;
    if (down && left)  return GAMEPAD_HAT_DOWN_LEFT;
    if (up && left)    return GAMEPAD_HAT_UP_LEFT;
    if (up)            return GAMEPAD_HAT_UP;
    if (down)          return GAMEPAD_HAT_DOWN;
    if (left)          return GAMEPAD_HAT_LEFT;
    if (right)         return GAMEPAD_HAT_RIGHT;
    return GAMEPAD_HAT_CENTERED;
}

void joystick_port2_host_in_init(void)
{
    setup_input(J2HI_UP);
    setup_input(J2HI_DOWN);
    setup_input(J2HI_LEFT);
    setup_input(J2HI_RIGHT);
    setup_input(J2HI_FIRE);
    setup_input(J2HI_B2);

    s_prev_hat = 0xff;
    s_prev_buttons = 0xffffffffu;
    s_last_report_us = time_us_32();

    printf("[joy2-in] Atari Port 2 digital joystick read active (2-button)\n");
}

void joystick_port2_host_in_task(void)
{
    bool up    = pressed(J2HI_UP);
    bool down  = pressed(J2HI_DOWN);
    bool left  = pressed(J2HI_LEFT);
    bool right = pressed(J2HI_RIGHT);
    bool fire  = pressed(J2HI_FIRE);
    bool b2    = pressed(J2HI_B2);

    // Opposite directions cancel (broken stick / noise).
    cancel_opposites(&up, &down);
    cancel_opposites(&left, &right);

    // Digital d-pad on left-stick axes (Chrome/macOS show these clearly as
    // Axis 0/1). Also keep hat for hosts that prefer HAT/DPAD (e.g. MiSTer).
    int8_t x = 0;
    int8_t y = 0;
    if (left)  x = -127;
    if (right) x = 127;
    if (up)    y = -127;  // HID: negative Y is up
    if (down)  y = 127;

    uint8_t hat = dirs_to_hat(up, down, left, right);
    uint32_t buttons = 0;
    if (fire) buttons |= GAMEPAD_BUTTON_A;
    if (b2)   buttons |= GAMEPAD_BUTTON_B;
    // Do NOT mirror dirs onto buttons 12–15: bit 12 is MODE (MiSTer OSD/menu
    // on many setups). Holding a direction then looks like Menu+D-pad and
    // tweaks autofire / opens OSD. Axes + hat are enough for d-pad.

    uint32_t now = time_us_32();
    bool due = (now - s_last_report_us) >= J2HI_REPORT_INTERVAL_US;
    bool first = (s_prev_hat == 0xff);

    // Periodic reports (not change-only) so browsers/macOS keep the pad listed.
    if (first || due) {
        s_prev_hat = hat;
        s_prev_buttons = buttons;
        s_last_report_us = now;
        usb_hid_device_send_gamepad(x, y, hat, buttons);
    }
}

#else

#include "joystick_port2_host_in.h"

void joystick_port2_host_in_init(void) {}
void joystick_port2_host_in_task(void) {}

#endif
