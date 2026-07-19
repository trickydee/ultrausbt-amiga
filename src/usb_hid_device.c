/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * USB HID device implementation — composite keyboard + mouse presented to a PC.
 */

#include "config.h"

#if ENABLE_USB_DEVICE_MODE

#include "usb_hid_device.h"

#include "tusb.h"
#include "pico/stdlib.h"
#include <string.h>

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE    = 2,
};

// Composite report descriptor: boot-style keyboard + relative mouse, distinguished
// by report ID so both share a single HID interface / IN endpoint.
static uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
};

//--------------------------------------------------------------------
// Descriptors
//--------------------------------------------------------------------

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_DEVICE_VID,
    .idProduct          = USB_DEVICE_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL = 1 };

#define EPNUM_HID        0x81
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report),
                       EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10),
};

static char const *string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },   // 0: language (English US)
    "amigahid-pico",                 // 1: manufacturer
    "Amiga Keyboard Bridge",         // 2: product
    "0001",                          // 3: serial
};

//--------------------------------------------------------------------
// Latched report state (written by producer, drained by task)
//--------------------------------------------------------------------

static volatile bool    s_kbd_dirty = false;
static volatile uint8_t s_kbd_mod = 0;
static volatile uint8_t s_kbd_keys[6] = { 0 };

static volatile bool    s_mouse_dirty = false;
static volatile uint8_t s_mouse_buttons = 0;
static volatile int8_t  s_mouse_x = 0, s_mouse_y = 0, s_mouse_wheel = 0;

static volatile uint8_t s_led_state = 0;

// caps-lock pulse: 0 idle, 1 send press, 2 held (waiting to release).
// macOS ignores very short Caps Lock presses (its caps-lock delay), so the
// synthetic key must be held down for a while before release.
#define CAPS_HOLD_MS 120
static volatile uint8_t s_caps_pulse_phase = 0;
static absolute_time_t  s_caps_press_time;

//--------------------------------------------------------------------
// Public producer API (called from the keyboard/mouse read paths)
//--------------------------------------------------------------------

void usb_hid_device_send_keyboard(uint8_t modifier, const uint8_t keycodes[6])
{
    s_kbd_mod = modifier;
    for (int i = 0; i < 6; i++) s_kbd_keys[i] = keycodes ? keycodes[i] : 0;
    s_kbd_dirty = true;
}

void usb_hid_device_send_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
    s_mouse_buttons = buttons;
    s_mouse_x = dx;
    s_mouse_y = dy;
    s_mouse_wheel = wheel;
    s_mouse_dirty = true;
}

void usb_hid_device_pulse_caps_lock(void)
{
    s_caps_pulse_phase = 1;
}

uint8_t usb_hid_device_led_state(void)
{
    return tud_mounted() ? s_led_state : 0;
}

//--------------------------------------------------------------------
// Report drainer
//--------------------------------------------------------------------

void usb_hid_device_task(void)
{
    if (!tud_mounted()) return;

    // caps-lock synthetic pulse takes priority so it is not clobbered by a normal
    // report while in flight. The key is held down for CAPS_HOLD_MS before release
    // because macOS ignores momentary Caps Lock presses.
    if (s_caps_pulse_phase != 0 && tud_hid_ready()) {
        if (s_caps_pulse_phase == 1) {
            uint8_t keys[6] = { HID_KEY_CAPS_LOCK, 0, 0, 0, 0, 0 };
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keys);
            s_caps_press_time = get_absolute_time();
            s_caps_pulse_phase = 2;
        } else if (absolute_time_diff_us(s_caps_press_time, get_absolute_time()) >= (CAPS_HOLD_MS * 1000)) {
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
            s_caps_pulse_phase = 0;
        }
        return;
    }

    if (s_kbd_dirty && tud_hid_ready()) {
        s_kbd_dirty = false;
        uint8_t keys[6];
        for (int i = 0; i < 6; i++) keys[i] = s_kbd_keys[i];
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, s_kbd_mod, keys);
        return;  // one report per servicing pass; mouse goes next pass
    }

    if (s_mouse_dirty && tud_hid_ready()) {
        s_mouse_dirty = false;
        tud_hid_mouse_report(REPORT_ID_MOUSE, s_mouse_buttons,
                             s_mouse_x, s_mouse_y, s_mouse_wheel, 0);
    }
}

//--------------------------------------------------------------------
// TinyUSB device callbacks
//--------------------------------------------------------------------

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return desc_hid_report;
}

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;
        const char *str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}

// Host -> device: GET_REPORT (rarely used for our IN-only reports).
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

// Host -> device: SET_REPORT — the PC sends keyboard LED state (caps/num/scroll).
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    if (report_type == HID_REPORT_TYPE_OUTPUT &&
        report_id == REPORT_ID_KEYBOARD && bufsize >= 1) {
        s_led_state = buffer[0];
    }
}

#endif // ENABLE_USB_DEVICE_MODE
