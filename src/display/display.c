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
static uint8_t button_debounce_counter = 0;

void display_init(void)
{
    // Setup the I2C interface to the display (same order as Atari code)
    i2c_init(SSD1306_I2C, 400000);
    gpio_set_function(SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA);
    gpio_pull_up(SSD1306_SCL);

    // Setup center button GPIO
    gpio_init(GPIO_BUTTON_MIDDLE);
    gpio_set_dir(GPIO_BUTTON_MIDDLE, GPIO_IN);
    gpio_pull_up(GPIO_BUTTON_MIDDLE);

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
    
    // AMIGA text (centered, scale 2x)
    ssd1306_draw_string(&disp, 25, 0, 2, (char*)"AMIGA");
    
    // Branding
    ssd1306_draw_string(&disp, 4, 24, 1, (char*)"amigahid-pico");
    
    // Version
    sprintf(version_buf, "v%d.%d.%d", SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, SOFTWARE_VERSION_PATCH);
    ssd1306_draw_string(&disp, 40, 40, 1, version_buf);
    
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
    
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
}

void display_update_devices(void)
{
    // If we're on the devices screen, update it
    if (current_screen == DISPLAY_SCREEN_DEVICES) {
        display_show_devices();
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
    // Read button state (active low - pressed when LOW, pulled high when released)
    bool state = gpio_get(GPIO_BUTTON_MIDDLE);
    
    // Button is pressed when GPIO is LOW (!state)
    if (!state) {
        // Button is pressed, increment counter
        // The <= means we go one past DEBOUNCE_COUNT and latch there until the button is released
        if (button_debounce_counter <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_debounce_counter == BUTTON_DEBOUNCE_COUNT) {
                // Button has been held long enough, trigger action
                // Toggle between splash and devices screen
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_devices();
                } else {
                    display_show_splash();
                }
            }
        }
    } else {
        // Button released, reset counter
        button_debounce_counter = 0;
    }
}

#else
// For non-Rev5 boards, provide stub implementations
void display_init(void) {}
void display_show_splash(void) {}
void display_show_devices(void) {}
void display_update_devices(void) {}
void display_get_counts(uint8_t *usb_kb, uint8_t *usb_mouse, uint8_t *usb_joy,
                       uint8_t *bt_kb, uint8_t *bt_mouse, uint8_t *bt_joy) {}
void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy) {}
void display_handle_buttons(void) {}
#endif // HIDPICO_REVISION == 5

