/**
 * Display interface for SSD1306 OLED
 */

#include "display/display.h"
#include "config.h"
#include "ssd1306.h"
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <stdio.h>
#include <string.h>

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#include "bluepad32_platform.h"
#endif

#if ENABLE_BLUEPAD32
#include "platform/amiga/joystick_port1.h"  // For joystick port 1 mode toggle
#endif

// Software version (from main.c)
// These are fallback values if not defined elsewhere - should match main.c
#ifndef SOFTWARE_VERSION_MAJOR
#define SOFTWARE_VERSION_MAJOR 1
#endif
#ifndef SOFTWARE_VERSION_MINOR
#define SOFTWARE_VERSION_MINOR 0
#endif
#ifndef SOFTWARE_VERSION_PATCH
#define SOFTWARE_VERSION_PATCH 1
#endif

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
static uint8_t button_middle_debounce = 0;
static uint8_t button_left_debounce = 0;
static uint8_t button_right_debounce = 0;

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
    char mode_buf[8];
    char mode_line[16];
    
    ssd1306_clear(&disp);
    
    // AMIGA text (centered, scale 2x)
    ssd1306_draw_string(&disp, 25, 0, 2, (char*)"AMIGA");
    
    // Branding
    ssd1306_draw_string(&disp, 4, 24, 1, (char*)"amigahid-pico");
    
    // Version
    sprintf(version_buf, "v%d.%d.%d", SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
    ssd1306_draw_string(&disp, 40, 40, 1, version_buf);
    
#if ENABLE_BLUEPAD32
    // Show USB/Bluetooth status on splash screen (bottom row)
    // Get runtime state (USB is always enabled, BT can be toggled)
    bool usb_enabled = true;  // USB is always enabled in this implementation
    bool bt_enabled = bluepad32_is_enabled();
    
    if (usb_enabled && bt_enabled) {
        sprintf(mode_buf, "USB+BT");
    }
    else if (usb_enabled) {
        sprintf(mode_buf, "USB");
    }
    else if (bt_enabled) {
        sprintf(mode_buf, "BT");
    }
    else {
        sprintf(mode_buf, "OFF");
    }

    // Show mode on bottom row with label
    sprintf(mode_line, "Mode %s", mode_buf);
    ssd1306_draw_string(&disp, 0, 55, 1, mode_line);
    
    // Show button label on splash screen
    // Right button: RST (Reset Bluetooth keys) - only show if BT is enabled
    if (bt_enabled) {
        ssd1306_draw_string(&disp, 100, 55, 1, (char*)"RST");
    }
#endif
    
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
}

void display_show_devices(void)
{
    char buf[32];
    
    ssd1306_clear(&disp);
    
    // Combined layout with aligned spacing:
    // Keybd   U X BT X
    // Mouse  U X BT X
    // Game    U X BT X
    // Use fixed-width labels (6 chars) so U/BT align properly
    sprintf(buf, "Keybd   U %d BT %d", usb_kb_count, bt_kb_count);
    ssd1306_draw_string(&disp, 0, 0, 1, buf);
    
    sprintf(buf, "Mouse   U %d BT %d", usb_mouse_count, bt_mouse_count);
    ssd1306_draw_string(&disp, 0, 9, 1, buf);
    
    sprintf(buf, "Game    U %d BT %d", usb_joy_count, bt_joy_count);
    ssd1306_draw_string(&disp, 0, 18, 1, buf);
    
#if ENABLE_BLUEPAD32
    // Show Port 1 mode status on bottom line
    bool is_joy_mode = amiga_joystick_port1_is_joystick_mode();
    sprintf(buf, "Port1: %s", is_joy_mode ? "JOY" : "MOUSE");
    ssd1306_draw_string(&disp, 0, 27, 1, buf);
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
    // Also update BT names screen if active
    if (current_screen == DISPLAY_SCREEN_BT_NAMES) {
        display_show_bt_names();
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
}

void display_handle_buttons(void)
{
    // Handle LEFT button
    bool left_state = gpio_get(GPIO_BUTTON_LEFT);
    if (!left_state) {
        if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_left_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
#if ENABLE_BLUEPAD32
                    // On splash screen: Toggle Bluetooth on/off (USB is always enabled)
                    bool bt_enabled = bluepad32_is_enabled();
                    
                    if (bt_enabled) {
                        // Bluetooth enabled -> disable it (USB only mode)
                        bluepad32_disable();
                        printf("Toggled to USB only mode\n");
                    } else {
                        // Bluetooth disabled -> enable it (USB + BT mode)
                        bluepad32_enable();
                        printf("Toggled to USB + Bluetooth mode\n");
                    }
                    
                    // Wait a moment for state to update
                    sleep_ms(50);
                    
                    // Refresh splash screen to show new mode
                    display_show_splash();
#endif
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
#if ENABLE_BLUEPAD32
                    // On devices screen: Toggle joystick port 1 mode
                    amiga_joystick_port1_toggle_mode();
                    bool is_joy_mode = amiga_joystick_port1_is_joystick_mode();
                    printf("Port 1 mode: %s\n", is_joy_mode ? "JOYSTICK" : "MOUSE");
                    // Refresh devices screen
                    display_show_devices();
#endif
                }
            }
        }
    } else {
        button_left_debounce = 0;
    }
    
    // Handle MIDDLE button (cycle through screens: SPLASH -> DEVICES -> BT_NAMES -> SPLASH)
    bool middle_state = gpio_get(GPIO_BUTTON_MIDDLE);
    if (!middle_state) {
        if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                // Button pressed - cycle through screens
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_devices();
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                    display_show_bt_names();
                } else if (current_screen == DISPLAY_SCREEN_BT_NAMES) {
                    display_show_splash();
                }
            }
        }
    } else {
        button_middle_debounce = 0;
    }
    
    // Handle RIGHT button (clear Bluetooth pairings on splash screen)
    bool right_state = gpio_get(GPIO_BUTTON_RIGHT);
    if (!right_state) {
        if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                // Button pressed - only act on splash screen
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
#if ENABLE_BLUEPAD32
                    if (bluepad32_is_enabled()) {
                        bluepad32_delete_pairing_keys();
                        printf("Bluetooth pairing keys deleted\n");
                        // Refresh splash screen
                        display_show_splash();
                    } else {
                        printf("Bluetooth not enabled\n");
                    }
#endif
                }
            }
        }
    } else {
        button_right_debounce = 0;
    }
}

void display_show_bt_names(void)
{
    char buf[64];
    const char* name;
    
    ssd1306_clear(&disp);
    
#if ENABLE_BLUEPAD32
    // Show first two joysticks
    name = bluepad32_get_device_name('J', 0);
    if (name) {
        // Truncate name to fit on screen (max ~20 chars)
        snprintf(buf, sizeof(buf), "J1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 0, 1, buf);
    } else {
        ssd1306_draw_string(&disp, 0, 0, 1, (char*)"J1: --");
    }
    
    name = bluepad32_get_device_name('J', 1);
    if (name) {
        snprintf(buf, sizeof(buf), "J2:%.20s", name);
        ssd1306_draw_string(&disp, 0, 9, 1, buf);
    } else {
        ssd1306_draw_string(&disp, 0, 9, 1, (char*)"J2: --");
    }
    
    // Show first keyboard
    name = bluepad32_get_device_name('K', 0);
    if (name) {
        snprintf(buf, sizeof(buf), "K1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 18, 1, buf);
    } else {
        ssd1306_draw_string(&disp, 0, 18, 1, (char*)"K1: --");
    }
    
    // Show first mouse
    name = bluepad32_get_device_name('M', 0);
    if (name) {
        snprintf(buf, sizeof(buf), "M1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 27, 1, buf);
    } else {
        ssd1306_draw_string(&disp, 0, 27, 1, (char*)"M1: --");
    }
#else
    ssd1306_draw_string(&disp, 0, 0, 1, (char*)"BT not enabled");
#endif
    
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_BT_NAMES;
}

#else
// For non-Rev5 boards, provide stub implementations
void display_init(void) {}
void display_show_splash(void) {}
void display_show_devices(void) {}
void display_show_bt_names(void) {}
void display_update_devices(void) {}
void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy) {}
void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_handle_buttons(void) {}
#endif // HIDPICO_REVISION == 5

