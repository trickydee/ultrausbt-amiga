/**
 * Display interface for SSD1306 OLED
 */

#include "display/display.h"
#include "config.h"
#include "usb_device_map.h"
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
#include "platform/amiga/joystick_port1.h"
#include "platform/amiga/cd32_pad.h"
#include "platform/amiga/port_mode.h"
#include "platform/amiga/quad_mouse.h"
// Forward declarations for Llamatron mode functions
extern bool usb_hid_get_llamatron_mode(void);
#endif

// Software version is defined in config.h (included above)

#if HIDPICO_REVISION == 5

// Global display instance
static ssd1306_t disp;

// Device counts
static uint8_t usb_kb_count = 0;
static uint8_t usb_mouse_count = 0;
static uint8_t usb_joy_count = 0;
static uint8_t bt_kb_count = 0;
static uint8_t bt_mouse_count = 0;
static uint8_t bt_joy_count = 0;

// Current screen
static display_screen_t current_screen = DISPLAY_SCREEN_SPLASH;

// Button handling
#define BUTTON_DEBOUNCE_COUNT 10
#define BT_PAIR_PRESS_MIN_MS 50
#define BT_WIPE_COMBO_HOLD_MS 5000
static uint8_t button_middle_debounce = 0;
static uint8_t button_left_debounce = 0;
static bool button_right_pressed = false;
static absolute_time_t button_right_press_start;
#if ENABLE_BLUEPAD32
static bool bt_wipe_combo_active = false;
static bool bt_wipe_combo_done = false;
static absolute_time_t bt_wipe_combo_start;
static uint32_t bt_wipe_overlay_last_seconds = UINT32_MAX;
static uint32_t splash_pair_countdown_last_seconds = UINT32_MAX;
#endif

#if ENABLE_BLUEPAD32
static void display_show_bt_clear_overlay(uint32_t seconds_left) {
    char countdown[20];
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 10, 2, (char*)"Pairing");
    ssd1306_draw_string(&disp, 0, 28, 2, (char*)"Clear");
    snprintf(countdown, sizeof(countdown), "%2lus", (unsigned long)seconds_left);
    ssd1306_draw_string(&disp, 96, 48, 1, countdown);
    ssd1306_show(&disp);
}
#endif

void display_init(void)
{
    // Setup the I2C interface to the display (same order as Atari code)
    i2c_init(SSD1306_I2C, 400000);
    gpio_set_function(SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA);
    gpio_pull_up(SSD1306_SCL);

    // Setup button GPIOs (all active low)
    gpio_init(GPIO_BUTTON_LEFT);
    gpio_set_dir(GPIO_BUTTON_LEFT, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_LEFT);
    
    gpio_init(GPIO_BUTTON_MIDDLE);
    gpio_set_dir(GPIO_BUTTON_MIDDLE, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_MIDDLE);
    
    gpio_init(GPIO_BUTTON_RIGHT);
    gpio_set_dir(GPIO_BUTTON_RIGHT, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_RIGHT);

    // Initialise the display library
    if (ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_ADDR, SSD1306_I2C)) {
        // Show splash screen on successful init
        display_show_splash();
    }
}

void display_show_splash(void)
{
    char version_buf[16];
    
    ssd1306_clear(&disp);
    
    // Show Port 1 mode as title (AMIGA, ATARI, JOYSTICK, or LLAMA, centered, scale 2x)
#if ENABLE_BLUEPAD32
    port1_mode_t port1_mode = port_mode_get_port1();
    const char* title;
    int x_pos;

    if (port1_mode == PORT1_MODE_LLAMA) {
        title = "LLAMA";
        x_pos = 25;
    } else if (port1_mode == PORT1_MODE_CD32) {
        title = "CD32";
        x_pos = 30;
    } else if (port1_mode == PORT1_MODE_JOY) {
        title = "JOYSTICK";
        x_pos = 5;
    } else {
        mouse_type_t mouse_type = amiga_quad_mouse_get_type();
        title = (mouse_type == MOUSE_TYPE_ATARI) ? "ATARI" : "AMIGA";
        x_pos = 25;
    }
    ssd1306_draw_string(&disp, x_pos, 0, 2, (char*)title);
#else
    ssd1306_draw_string(&disp, 25, 0, 2, (char*)"AMIGA");
#endif
    
    // Branding
    ssd1306_draw_string(&disp, 4, 24, 1, (char*)"ultramegausb.com");
    
    // Version
    sprintf(version_buf, "v%d.%d.%d", SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
    ssd1306_draw_string(&disp, 40, 40, 1, version_buf);
    
#if ENABLE_BLUEPAD32
    // Show pairing status / action on splash screen.
    char pair_status[20];
    bool bt_enabled = bluepad32_is_enabled();
    uint32_t secs = 0;
    if (bluepad32_pairing_is_active()) {
        secs = bluepad32_pairing_remaining_seconds();
        if (secs > 0) {
            snprintf(pair_status, sizeof(pair_status), "Pair ON %lus", (unsigned long)secs);
        } else {
            snprintf(pair_status, sizeof(pair_status), "Pair ON");
        }
    } else {
        snprintf(pair_status, sizeof(pair_status), "Pair OFF");
    }
    ssd1306_draw_string(&disp, 0, 55, 1, pair_status);
    if (bt_enabled) {
        ssd1306_draw_string(&disp, 96, 55, 1, (char*)"PAIR");
    }
    splash_pair_countdown_last_seconds = secs;
#endif
    
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
}

void display_show_devices(void)
{
    char buf[32];
    
    ssd1306_clear(&disp);
    
    // Title at the top
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"Devices");
    
    // Combined layout with aligned spacing:
    // Keybd   U X BT X
    // Mouse  U X BT X
    // Game    U X BT X
    // Use fixed-width labels (6 chars) so U/BT align properly
    sprintf(buf, "Keybd   U %d BT %d", usb_kb_count, bt_kb_count);
    ssd1306_draw_string(&disp, 0, 9, 1, buf);
    
    sprintf(buf, "Mouse   U %d BT %d", usb_mouse_count, bt_mouse_count);
    ssd1306_draw_string(&disp, 0, 18, 1, buf);
    
    sprintf(buf, "Game    U %d BT %d", usb_joy_count, bt_joy_count);
    ssd1306_draw_string(&disp, 0, 27, 1, buf);
    
#if ENABLE_BLUEPAD32
    sprintf(buf, "Port1:  %s", port_mode_port1_label());
    ssd1306_draw_string(&disp, 0, 36, 1, buf);
    if (port_mode_get_port1() == PORT1_MODE_MOUSE) {
        mouse_type_t mouse_type = amiga_quad_mouse_get_type();
        sprintf(buf, "Type:   %s", mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
        ssd1306_draw_string(&disp, 0, 45, 1, buf);
    }
#endif
#if HIDPICO_REVISION == 5
    sprintf(buf, "Port2:  %s", port_mode_get_port2_cd32() ? "CD32" : "STD");
    ssd1306_draw_string(&disp, 0, 55, 1, buf);
#endif
    
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
}

void display_update_devices(void)
{
    // If we're on the devices screen, update it
    if (current_screen == DISPLAY_SCREEN_DEVICES) {
        display_show_devices();
    }
    // Also update Map Devices screen if active
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
    
    // Refresh splash screen if it's active (to update mode display)
    if (current_screen == DISPLAY_SCREEN_SPLASH) {
        display_show_splash();
    }
}

void display_show_controller_detected(const char* controller_name, const char* controller_model, uint32_t duration_ms)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 25, 10, 2, (char*)controller_name);
    if (controller_model) {
        ssd1306_draw_string(&disp, 10, 35, 1, (char*)controller_model);
    }
    ssd1306_show(&disp);
    sleep_ms(duration_ms);
    /* Restore splash screen after the delay so the controller message clears automatically */
    display_show_splash();
}

void display_handle_buttons(void)
{
    // Handle LEFT button
    bool left_state = gpio_get(GPIO_BUTTON_LEFT);
    bool right_state = gpio_get(GPIO_BUTTON_RIGHT);
#if ENABLE_BLUEPAD32
    // Left+Right combo: hold 5s to clear Bluetooth pairing keys.
    if (!left_state && !right_state) {
        if (!bt_wipe_combo_active) {
            bt_wipe_combo_active = true;
            bt_wipe_combo_done = false;
            bt_wipe_combo_start = get_absolute_time();
            bt_wipe_overlay_last_seconds = 5;
            display_show_bt_clear_overlay(5);
        } else if (!bt_wipe_combo_done) {
            uint32_t combo_ms = (uint32_t)(absolute_time_diff_us(bt_wipe_combo_start, get_absolute_time()) / 1000);
            uint32_t remaining_ms = (combo_ms >= BT_WIPE_COMBO_HOLD_MS) ? 0 : (BT_WIPE_COMBO_HOLD_MS - combo_ms);
            uint32_t remaining_s = (remaining_ms + 999) / 1000;
            if (remaining_s != bt_wipe_overlay_last_seconds) {
                bt_wipe_overlay_last_seconds = remaining_s;
                display_show_bt_clear_overlay(remaining_s);
            }
            if (combo_ms >= BT_WIPE_COMBO_HOLD_MS) {
                bluepad32_delete_pairing_keys();
                printf("Bluetooth pairing keys deleted\n");
                bt_wipe_combo_done = true;
                bt_wipe_overlay_last_seconds = UINT32_MAX;
                display_show_splash();
            }
        }
        button_left_debounce = 0;
        button_right_pressed = false;
        return;
    } else {
        bool combo_was_active = bt_wipe_combo_active;
        bt_wipe_combo_active = false;
        bt_wipe_combo_done = false;
        bt_wipe_overlay_last_seconds = UINT32_MAX;
        if (combo_was_active) {
            if (current_screen == DISPLAY_SCREEN_SPLASH) {
                display_show_splash();
            } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                display_show_devices();
            } else if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
                display_show_map_devices();
            }
        }
    }
#endif

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
    
    // Handle MIDDLE button (cycle through screens: SPLASH -> DEVICES -> MAP_DEVICES -> SPLASH)
    bool middle_state = gpio_get(GPIO_BUTTON_MIDDLE);
    if (!middle_state) {
        if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                // Button pressed - cycle through screens
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_devices();
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                    display_show_map_devices();
                } else if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
                    display_show_splash();
                }
            }
        }
    } else {
        button_middle_debounce = 0;
    }
    
    // Handle RIGHT button (single button only)
    if (!right_state && !button_right_pressed) {
        button_right_pressed = true;
        button_right_press_start = get_absolute_time();
    } else if (right_state && button_right_pressed) {
        button_right_pressed = false;
        uint32_t press_ms = (uint32_t)(absolute_time_diff_us(button_right_press_start, get_absolute_time()) / 1000);
        if (press_ms < BT_PAIR_PRESS_MIN_MS) {
            return;
        }
        if (current_screen == DISPLAY_SCREEN_SPLASH) {
#if ENABLE_BLUEPAD32
            if (!bluepad32_is_enabled()) {
                printf("Bluetooth not enabled\n");
                return;
            }
            if (bluepad32_pairing_is_active()) {
                bluepad32_pairing_stop();
                printf("Bluetooth pairing OFF\n");
            } else {
                bluepad32_pairing_start();
                printf("Bluetooth pairing ON\n");
            }
            display_show_splash();
#endif
        } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
            // On devices screen: Toggle mouse type if Port 1 is in mouse mode
#if ENABLE_BLUEPAD32
            bool is_joy_mode = amiga_joystick_port1_is_joystick_mode();
            if (!is_joy_mode) {
                amiga_quad_mouse_toggle_type();
                mouse_type_t mouse_type = amiga_quad_mouse_get_type();
                printf("Mouse type: %s\n", mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
                display_show_devices();
            }
#endif
        }
    }
}

void display_tick(void)
{
#if ENABLE_BLUEPAD32
    if (current_screen != DISPLAY_SCREEN_SPLASH || bt_wipe_combo_active || !bluepad32_is_enabled()) {
        return;
    }
    uint32_t secs = bluepad32_pairing_remaining_seconds();
    if (secs != splash_pair_countdown_last_seconds) {
        display_show_splash();
    }
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

    // J2 = first gamepad slot (port 2); J1 = second (port 1 when in joystick mode)
    display_draw_map_line(9, "J2", bt_j2, usb_map_get_gamepad(0));
    display_draw_map_line(18, "J1", bt_j1, usb_map_get_gamepad(1));
    display_draw_map_line(27, "K1", bt_k1, usb_map_get_keyboard());
    display_draw_map_line(36, "M1", bt_m1, usb_map_get_mouse());

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_MAP_DEVICES;
}

#else
// For non-Rev5 boards, provide stub implementations
void display_init(void) {}
void display_show_splash(void) {}
void display_show_devices(void) {}
void display_show_map_devices(void) {}
void display_update_devices(void) {}
void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy) {}
void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_handle_buttons(void) {}
void display_tick(void) {}
void display_show_controller_detected(const char* controller_name, const char* controller_model, uint32_t duration_ms) {}
#endif // HIDPICO_REVISION == 5

