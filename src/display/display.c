/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Display interface for SSD1306 OLED — screen carousel + Settings page.
 * See doc/archive/oled-ui-style-guide.md
 */

#include "display/display.h"
#include "config.h"
#include "usb_device_map.h"
#include "usb_mode.h"
#include "ssd1306.h"
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#include "bluepad32_platform.h"
#endif

#if ENABLE_BLUEPAD32
#include "platform/amiga/port_mode.h"
#endif

#if HIDPICO_REV_ATARI_BOARD

static ssd1306_t disp;

static uint8_t usb_kb_count = 0;
static uint8_t usb_mouse_count = 0;
static uint8_t usb_joy_count = 0;
static uint8_t bt_kb_count = 0;
static uint8_t bt_mouse_count = 0;
static uint8_t bt_joy_count = 0;

static display_screen_t current_screen = DISPLAY_SCREEN_SPLASH;

#define BUTTON_DEBOUNCE_COUNT 10
#define BT_PAIR_PRESS_MIN_MS 50

static uint8_t button_middle_debounce = 0;
static uint8_t button_left_debounce = 0;
static uint8_t button_right_debounce = 0;
static bool button_right_pressed = false;
static absolute_time_t button_right_press_start;

#if ENABLE_BLUEPAD32
static uint32_t splash_pair_countdown_last_seconds = UINT32_MAX;
static bool bt_pair_combo_latched = false;  /* Middle+Left edge already handled */
#endif

/* Settings carousel page: in-page select */
enum {
    SETTINGS_SEL_CLEAR_PAIR = 0,
    SETTINGS_SEL_PAIRING = 1,
    SETTINGS_SEL_USB_MODE = 2,
    SETTINGS_SEL_BACK = 3,
};
static uint8_t settings_sel = SETTINGS_SEL_BACK;
static bool settings_confirm_clear = false;

static bool display_in_pc_kbd_mode(void)
{
#if ENABLE_USB_DEVICE_MODE
    return usb_mode_is_device();
#else
    return false;
#endif
}

#if ENABLE_BLUEPAD32
static void display_show_clear_pair_confirm(void)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 10, 1, (char*)"Clear pairings?");
    ssd1306_draw_string(&disp, 0, 28, 1, (char*)"#=yes  Down=no");
    ssd1306_show(&disp);
}
#endif

static void settings_sel_clamp(void)
{
    if (display_in_pc_kbd_mode() &&
        (settings_sel == SETTINGS_SEL_CLEAR_PAIR || settings_sel == SETTINGS_SEL_PAIRING)) {
        settings_sel = SETTINGS_SEL_BACK;
    }
}

/** Enter Settings with Back as the default selection. */
static void display_enter_settings(void)
{
    settings_sel = SETTINGS_SEL_BACK;
    display_show_settings();
}

void display_show_settings(void)
{
    char line[22];
    int y = 12;

    settings_confirm_clear = false;
    settings_sel_clamp();

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Settings");

#if ENABLE_BLUEPAD32
    if (!display_in_pc_kbd_mode()) {
        snprintf(line, sizeof(line), "%s Clear BT pair",
                 (settings_sel == SETTINGS_SEL_CLEAR_PAIR) ? ">" : " ");
        ssd1306_draw_string(&disp, 0, y, 1, line);
        y += 10;

        /* Label is the pairing state you will switch to. */
        snprintf(line, sizeof(line), "%s Pair %s",
                 (settings_sel == SETTINGS_SEL_PAIRING) ? ">" : " ",
                 bluepad32_pairing_is_active() ? "OFF" : "ON");
        ssd1306_draw_string(&disp, 0, y, 1, line);
        y += 10;
    }
#endif

#if ENABLE_USB_DEVICE_MODE
    {
        /* Label is the role you will switch to (not the current role). */
        bool device = usb_mode_is_device();
        snprintf(line, sizeof(line), "%s %s",
                 (settings_sel == SETTINGS_SEL_USB_MODE) ? ">" : " ",
                 device ? "Device Mode" : "Host Mode");
        ssd1306_draw_string(&disp, 0, y, 1, line);
        y += 10;
    }
#else
    (void)line;
#endif

    snprintf(line, sizeof(line), "%s Back",
             (settings_sel == SETTINGS_SEL_BACK) ? ">" : " ");
    ssd1306_draw_string(&disp, 0, y, 1, line);

    ssd1306_draw_string(&disp, 0, 55, 1, (char*)"#=ok ^/v=sel");
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SETTINGS;
}

static void settings_activate(void)
{
    if (settings_sel == SETTINGS_SEL_BACK) {
        display_show_splash();
        return;
    }
#if ENABLE_BLUEPAD32
    if (settings_sel == SETTINGS_SEL_CLEAR_PAIR && !display_in_pc_kbd_mode()) {
        settings_confirm_clear = true;
        display_show_clear_pair_confirm();
        return;
    }
    if (settings_sel == SETTINGS_SEL_PAIRING && !display_in_pc_kbd_mode()) {
        if (bluepad32_is_enabled()) {
            if (bluepad32_pairing_is_active()) {
                bluepad32_pairing_stop();
                printf("Bluetooth pairing OFF\n");
            } else {
                bluepad32_pairing_start();
                printf("Bluetooth pairing ON\n");
            }
            display_show_settings();
        } else {
            printf("Bluetooth not enabled\n");
        }
        return;
    }
#endif
#if ENABLE_USB_DEVICE_MODE
    if (settings_sel == SETTINGS_SEL_USB_MODE) {
        printf("[USBMODE] Settings: toggle USB role\n");
        usb_mode_request_toggle();  /* persists + reboots, does not return */
    }
#endif
}

static void settings_confirm_yes(void)
{
#if ENABLE_BLUEPAD32
    if (settings_confirm_clear) {
        bluepad32_delete_pairing_keys();
        printf("Bluetooth pairing keys deleted\n");
        settings_confirm_clear = false;
        display_show_settings();
    }
#else
    (void)0;
#endif
}

static void settings_confirm_no(void)
{
    if (settings_confirm_clear) {
        settings_confirm_clear = false;
        display_show_settings();
    }
}

static void settings_move_sel(int delta)
{
    /* Build ordered list of valid selections for current mode */
    uint8_t opts[4];
    uint8_t n = 0;
#if ENABLE_BLUEPAD32
    if (!display_in_pc_kbd_mode()) {
        opts[n++] = SETTINGS_SEL_CLEAR_PAIR;
        opts[n++] = SETTINGS_SEL_PAIRING;
    }
#endif
#if ENABLE_USB_DEVICE_MODE
    opts[n++] = SETTINGS_SEL_USB_MODE;
#endif
    opts[n++] = SETTINGS_SEL_BACK;
    if (n == 0) {
        return;
    }

    int idx = 0;
    bool found = false;
    for (uint8_t i = 0; i < n; i++) {
        if (opts[i] == settings_sel) {
            idx = (int)i;
            found = true;
            break;
        }
    }
    if (!found) {
        idx = (int)n - 1;  /* default to Back */
    }

    idx += delta;
    while (idx < 0) {
        idx += (int)n;
    }
    idx %= (int)n;

    settings_sel = opts[idx];
    display_show_settings();
}

void display_init(void)
{
    i2c_init(SSD1306_I2C, 400000);
    gpio_set_function(SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA);
    gpio_pull_up(SSD1306_SCL);

    gpio_init(GPIO_BUTTON_LEFT);
    gpio_set_dir(GPIO_BUTTON_LEFT, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_LEFT);

    gpio_init(GPIO_BUTTON_MIDDLE);
    gpio_set_dir(GPIO_BUTTON_MIDDLE, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_MIDDLE);

    gpio_init(GPIO_BUTTON_RIGHT);
    gpio_set_dir(GPIO_BUTTON_RIGHT, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_RIGHT);

    if (ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_ADDR, SSD1306_I2C)) {
        display_show_splash();
    }
}

void display_show_splash(void)
{
    char version_buf[16];
    char line[20];

#if ENABLE_USB_DEVICE_MODE
    if (usb_mode_is_device()) {
        ssd1306_clear(&disp);
        ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Host Mode");
        ssd1306_draw_string(&disp, 0, 16, 1, (char*)"Amiga kbd -> USB");
        ssd1306_draw_string(&disp, 0, 28, 1, (char*)"# Settings");
        sprintf(version_buf, "v%d.%d.%d", SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
        ssd1306_draw_string(&disp, 0, 52, 1, version_buf);
        ssd1306_show(&disp);
        current_screen = DISPLAY_SCREEN_SPLASH;
        return;
    }
#endif

    ssd1306_clear(&disp);

#if ENABLE_BLUEPAD32
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Device Mode");
    snprintf(line, sizeof(line), "1:%s", port_mode_port1_label());
    ssd1306_draw_string(&disp, 0, 16, 2, line);
    snprintf(line, sizeof(line), "2:%s", port_mode_port2_label());
    ssd1306_draw_string(&disp, 0, 34, 2, line);
#else
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Device Mode");
    (void)line;
#endif

    sprintf(version_buf, "v%d.%d.%d", SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
#if ENABLE_BLUEPAD32
    char pair_status[20];
    uint32_t secs = 0;
    if (bluepad32_pairing_is_active()) {
        secs = bluepad32_pairing_remaining_seconds();
        if (secs > 0) {
            snprintf(pair_status, sizeof(pair_status), "Pair ON %lu ", (unsigned long)secs);
        } else {
            snprintf(pair_status, sizeof(pair_status), "Pair ON");
        }
    } else {
        snprintf(pair_status, sizeof(pair_status), "Pair OFF");
    }
    ssd1306_draw_string(&disp, 0, 55, 1, pair_status);
    {
        int ver_x = 128 - ((int)strlen(version_buf) * 6) - 12;
        if (ver_x < 0) {
            ver_x = 0;
        }
        ssd1306_draw_string(&disp, ver_x, 55, 1, version_buf);
    }
    splash_pair_countdown_last_seconds = secs;
#else
    ssd1306_draw_string(&disp, 40, 40, 1, version_buf);
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
}

void display_show_devices(void)
{
    char buf[32];

    if (display_in_pc_kbd_mode()) {
        display_show_splash();
        return;
    }

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Devices");

    sprintf(buf, "Keybd   U %d BT %d", usb_kb_count, bt_kb_count);
    ssd1306_draw_string(&disp, 0, 9, 1, buf);

    sprintf(buf, "Mouse   U %d BT %d", usb_mouse_count, bt_mouse_count);
    ssd1306_draw_string(&disp, 0, 18, 1, buf);

    sprintf(buf, "Game    U %d BT %d", usb_joy_count, bt_joy_count);
    ssd1306_draw_string(&disp, 0, 27, 1, buf);

#if ENABLE_BLUEPAD32
    sprintf(buf, "Port1:  %s", port_mode_port1_label());
    ssd1306_draw_string(&disp, 0, 36, 1, buf);
#endif
#if HIDPICO_REV_ATARI_BOARD
    sprintf(buf, "Port2:  %s", port_mode_port2_label());
    ssd1306_draw_string(&disp, 0, 45, 1, buf);
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
}

void display_update_devices(void)
{
    if (display_in_pc_kbd_mode()) {
        return;
    }
    if (current_screen == DISPLAY_SCREEN_DEVICES) {
        display_show_devices();
    }
    if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
        display_show_map_devices();
    }
}

void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy)
{
    if (usb_kb) *usb_kb = usb_kb_count;
    if (usb_mouse) *usb_mouse = usb_mouse_count;
    if (usb_joy) *usb_joy = usb_joy_count;
    if (bt_kb) *bt_kb = bt_kb_count;
    if (bt_mouse) *bt_mouse = bt_mouse_count;
    if (bt_joy) *bt_joy = bt_joy_count;
}

void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    usb_kb_count = kb;
    usb_mouse_count = mouse;
    usb_joy_count = joy;
    display_update_devices();
}

void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    bt_kb_count = kb;
    bt_mouse_count = mouse;
    bt_joy_count = joy;
    display_update_devices();

    if (current_screen == DISPLAY_SCREEN_SPLASH && !display_in_pc_kbd_mode()) {
        display_show_splash();
    }
}

void display_show_controller_detected(const char* controller_name, const char* controller_model, uint32_t duration_ms)
{
    if (display_in_pc_kbd_mode()) {
        return;
    }
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 25, 10, 2, (char*)controller_name);
    if (controller_model) {
        ssd1306_draw_string(&disp, 10, 35, 1, (char*)controller_model);
    }
    ssd1306_show(&disp);
    sleep_ms(duration_ms);
    display_show_splash();
}

void display_handle_buttons(void)
{
    bool left_state = gpio_get(GPIO_BUTTON_LEFT);    /* Up — active low: pressed == 0 */
    bool right_state = gpio_get(GPIO_BUTTON_RIGHT);  /* Down */
    bool middle_state = gpio_get(GPIO_BUTTON_MIDDLE); /* # */

#if ENABLE_USB_DEVICE_MODE
    /* PC KBD mode: only Splash ↔ Settings carousel + Settings actions */
    if (usb_mode_is_device()) {
        if (settings_confirm_clear) {
            settings_confirm_clear = false;
        }

        if (current_screen == DISPLAY_SCREEN_SETTINGS) {
            if (!left_state) {
                if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT) {
                    if (++button_left_debounce == BUTTON_DEBOUNCE_COUNT) {
                        settings_move_sel(-1);
                    }
                }
            } else {
                button_left_debounce = 0;
            }

            if (!right_state) {
                if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
                    if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                        settings_move_sel(1);
                    }
                }
            } else {
                button_right_debounce = 0;
            }

            if (!middle_state) {
                if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
                    if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                        settings_activate();
                    }
                }
            } else {
                button_middle_debounce = 0;
            }
            return;
        }

        /* Splash: # → Settings */
        if (!middle_state) {
            if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
                if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                    display_enter_settings();
                }
            }
        } else {
            button_middle_debounce = 0;
        }
        button_left_debounce = 0;
        button_right_debounce = 0;
        return;
    }
#endif

#if ENABLE_BLUEPAD32
    /* Middle + Left: toggle Bluetooth pairing (edge-triggered). */
    if (!middle_state && !left_state) {
        if (!bt_pair_combo_latched) {
            bt_pair_combo_latched = true;
            if (bluepad32_is_enabled()) {
                if (bluepad32_pairing_is_active()) {
                    bluepad32_pairing_stop();
                    printf("Bluetooth pairing OFF\n");
                } else {
                    bluepad32_pairing_start();
                    printf("Bluetooth pairing ON\n");
                }
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_splash();
                }
            } else {
                printf("Bluetooth not enabled\n");
            }
        }
        button_left_debounce = 0;
        button_middle_debounce = 0;
        button_right_debounce = 0;
        button_right_pressed = false;
        return;
    }
    bt_pair_combo_latched = false;
#endif

    /* Settings page: confirm overlay, in-page select, or # activate */
    if (current_screen == DISPLAY_SCREEN_SETTINGS) {
        if (settings_confirm_clear) {
            if (!middle_state) {
                if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
                    if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                        settings_confirm_yes();
                    }
                }
            } else {
                button_middle_debounce = 0;
            }
            if (!right_state) {
                if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
                    if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                        settings_confirm_no();
                    }
                }
            } else {
                button_right_debounce = 0;
            }
            button_left_debounce = 0;
            return;
        }

        if (!left_state) {
            if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT) {
                if (++button_left_debounce == BUTTON_DEBOUNCE_COUNT) {
                    settings_move_sel(-1);
                }
            }
        } else {
            button_left_debounce = 0;
        }

        if (!right_state) {
            if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
                if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                    settings_move_sel(1);
                }
            }
        } else {
            button_right_debounce = 0;
        }

        if (!middle_state) {
            if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
                if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                    settings_activate();
                }
            }
        } else {
            button_middle_debounce = 0;
        }
        return;
    }

    /* Up (Left): Port 1 cycle on Splash / Devices */
    if (!left_state) {
        if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_left_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
#if ENABLE_BLUEPAD32
                    port_mode_cycle_port1();
                    display_show_splash();
#endif
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
#if ENABLE_BLUEPAD32
                    port_mode_cycle_port1();
                    display_show_devices();
#endif
                }
            }
        }
    } else {
        button_left_debounce = 0;
    }

    /* # (Middle): advance screen carousel */
    if (!middle_state) {
        if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_devices();
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                    display_show_map_devices();
                } else if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
                    display_enter_settings();
                } else {
                    display_show_splash();
                }
            }
        }
    } else {
        button_middle_debounce = 0;
    }

    /* Down (Right): Port 2 cycle on Splash / Devices */
    if (!right_state && !button_right_pressed) {
        button_right_pressed = true;
        button_right_press_start = get_absolute_time();
    } else if (right_state && button_right_pressed) {
        button_right_pressed = false;
        uint32_t press_ms = (uint32_t)(absolute_time_diff_us(button_right_press_start, get_absolute_time()) / 1000);
        if (press_ms < BT_PAIR_PRESS_MIN_MS) {
            return;
        }
        if (current_screen == DISPLAY_SCREEN_SPLASH || current_screen == DISPLAY_SCREEN_DEVICES) {
#if ENABLE_BLUEPAD32
            port_mode_cycle_port2();
            if (current_screen == DISPLAY_SCREEN_SPLASH) {
                display_show_splash();
            } else {
                display_show_devices();
            }
#endif
        }
    }
}

void display_tick(void)
{
#if ENABLE_BLUEPAD32
    if (current_screen != DISPLAY_SCREEN_SPLASH || display_in_pc_kbd_mode() || !bluepad32_is_enabled()) {
        return;
    }
    uint32_t secs = bluepad32_pairing_remaining_seconds();
    if (secs != splash_pair_countdown_last_seconds) {
        display_show_splash();
    }
#else
    (void)0;
#endif
}

static void display_draw_map_line(int y, const char* label, const char* bt_name, const char* usb_name)
{
    char buf[64];
    const char* name = bt_name;
    if (!name) {
        name = usb_name;
    }
    if (name) {
        snprintf(buf, sizeof(buf), "%s:%.20s", label, name);
    } else {
        snprintf(buf, sizeof(buf), "%s: --", label);
    }
    ssd1306_draw_string(&disp, 0, y, 1, buf);
}

void display_show_map_devices(void)
{
    if (display_in_pc_kbd_mode()) {
        display_show_splash();
        return;
    }

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Map Devices");

    const char* bt_j2 = NULL;
    const char* bt_j1 = NULL;
    const char* bt_k1 = NULL;
    const char* bt_m1 = NULL;
#if ENABLE_BLUEPAD32
    bt_j2 = bluepad32_get_device_name('J', 0);
    bt_j1 = bluepad32_get_device_name('J', 1);
    bt_k1 = bluepad32_get_device_name('K', 0);
    bt_m1 = bluepad32_get_device_name('M', 0);
#endif

    display_draw_map_line(9, "J2", bt_j2, usb_map_get_gamepad(0));
    display_draw_map_line(18, "J1", bt_j1, usb_map_get_gamepad(1));
    display_draw_map_line(27, "K1", bt_k1, usb_map_get_keyboard());
    display_draw_map_line(36, "M1", bt_m1, usb_map_get_mouse());

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_MAP_DEVICES;
}

#else
void display_init(void) {}
void display_show_splash(void) {}
void display_show_devices(void) {}
void display_show_map_devices(void) {}
void display_show_settings(void) {}
void display_update_devices(void) {}
void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy) {}
void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_handle_buttons(void) {}
void display_tick(void) {}
void display_show_controller_detected(const char* controller_name, const char* controller_model, uint32_t duration_ms) {}
#endif // HIDPICO_REV_ATARI_BOARD
