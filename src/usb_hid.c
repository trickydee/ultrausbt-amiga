/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * human interface device handling.
 *
 * if this file looks kind of sketchy, i'm just at the beginning of my
 * understanding of the tinyusb stack. apologies whilst i get to grips with
 * the terminology, or mapping it to what i understand of it.
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"

// other includes
#include <stdint.h>

#include "tusb_config.h"
#include "platform/amiga/keyboard_serial_io.h"  // amiga only, for now, until i get hold of an ST :D
#include "platform/amiga/keyboard.h"
#include "platform/amiga/quad_mouse.h"
#include "platform/amiga/joystick_port1.h"
#include "platform/amiga/joystick_port2.h"
#include "util/output.h"
#include "util/debug_cons.h"
#include "display/display.h"

#if ENABLE_BLUEPAD32
#include "bluepad32_platform.h"
#endif

// maximum number of reports per hid device
#define MAX_REPORT 4

// Llamatron twinstick mode state
// When enabled, a single gamepad's left stick controls Port 1, right stick controls Port 2
static bool llamatron_mode = false;
static bool llamatron_active = false;  // True when mode is active and conditions are met
static bool llamatron_restore_joystick_mode = false;  // Track if port 1 was in joystick mode before enabling Llamatron

// repetitive modifier check macros (@todo probably better iterated in future?)
#define _SINGLE_MOD_CHECK(hid_mod) \
    if ((report->modifier & hid_mod) && !(last_report.modifier & hid_mod)) \
        amiga_hid_modifier(hid_mod, false); \
    if (!(report->modifier & hid_mod) && (last_report.modifier & hid_mod)) \
        amiga_hid_modifier(hid_mod, true);

#define _MULTI_MOD_CHECK(hid_mod_a, hid_mod_b) \
    if (((report->modifier & hid_mod_a) && !(last_report.modifier & hid_mod_a)) \
        || ((report->modifier & hid_mod_b) && !(last_report.modifier & hid_mod_b)) \
    ) \
        amiga_hid_modifier(hid_mod_a, false); \
    if ((!(report->modifier & hid_mod_a) && (last_report.modifier & hid_mod_a)) \
        || (!(report->modifier & hid_mod_b) && (last_report.modifier & hid_mod_b)) \
    ) \
        amiga_hid_modifier(hid_mod_a, true);

// textual representations of attached devices
const uint8_t hid_protocol_type[] = { AP_H_UNKNOWN, AP_H_KEYBOARD, AP_H_MOUSE };

// hid information structure
static struct _hid_info
{
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
} hid_info[CFG_TUH_HID];

static void process_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);
static void handle_event_keyboard(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report);
static void handle_event_mouse(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const *report);
static void handle_event_gamepad(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);

// Track first gamepad device (mapped to joystick port 2)
static uint8_t first_gamepad_dev_addr = 0;
static uint8_t first_gamepad_instance = 0;

#if HIDPICO_REVISION == 5
// USB device counts for display
static uint8_t usb_kb_count = 0;
static uint8_t usb_mouse_count = 0;
static uint8_t usb_joy_count = 0;

// Forward declaration
static void update_usb_device_counts(void);
#endif

void hid_app_task(void)
{
    // null function to satisfy stack
}

// callback functions; methods below suffixed with "_cb" are called by tinyusb
// when processing certain hid events.

/**
 * HID connection callback
 *
 * @param dev_addr    Address of connected device
 * @param instance    Instance of connected device
 * @param desc_report Report descriptor
 * @param desc_len    Length of descriptor
 */
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len)
{
    uint8_t hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    dbgcons_plug(hid_protocol_type[hid_protocol]);

    // this part doesn't entirely make sense to me; hid devices come in two modes, boot protocol and report;
    // as i understand it, boot proto is intended for simplistic software such as bios which don't want to
    // implement a full stack. so if we're not in boot proto mode, display... something?
    // this might be number of interfaces on a device (think wireless kbd+mouse receiver). maybe. speculation.
    if (hid_protocol == HID_ITF_PROTOCOL_NONE) {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info, MAX_REPORT, desc_report, desc_len);
        // ahprintf("[PLUG] %02x report(s)\n", hid_info[instance].report_count);
    }

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        // ahprintf("[PLUG] warning! report request failed; delayed initialisation?\n");
    }
    
#if HIDPICO_REVISION == 5
    // Update device counts
    if (hid_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        usb_kb_count++;
    } else if (hid_protocol == HID_ITF_PROTOCOL_MOUSE) {
        usb_mouse_count++;
    } else if (hid_protocol == HID_ITF_PROTOCOL_NONE) {
        // Could be a gamepad/joystick (non-boot protocol)
        // Track first gamepad
        if (first_gamepad_dev_addr == 0) {
            first_gamepad_dev_addr = dev_addr;
            first_gamepad_instance = instance;
            usb_joy_count++;
        }
    }
    update_usb_device_counts();
#endif
}

/**
 * HID disconnection callback
 *
 * @param dev_addr    Address of disconnected device
 * @param instance    Instance of disconnected device
 */
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    uint8_t hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    dbgcons_unplug(hid_protocol_type[hid_protocol]);
    
    // Clear first gamepad if it was disconnected
    if (first_gamepad_dev_addr == dev_addr && first_gamepad_instance == instance) {
        first_gamepad_dev_addr = 0;
        first_gamepad_instance = 0;
        amiga_joystick_port2_reset();
    }
    
#if HIDPICO_REVISION == 5
    // Update device counts
    if (hid_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        if (usb_kb_count > 0) usb_kb_count--;
    } else if (hid_protocol == HID_ITF_PROTOCOL_MOUSE) {
        if (usb_mouse_count > 0) usb_mouse_count--;
    } else if (hid_protocol == HID_ITF_PROTOCOL_NONE) {
        if (dev_addr == first_gamepad_dev_addr && instance == first_gamepad_instance) {
            if (usb_joy_count > 0) usb_joy_count--;
        }
    }
    update_usb_device_counts();
#endif
}

#if HIDPICO_REVISION == 5
// Update display with current USB device counts
static void update_usb_device_counts(void)
{
    display_set_usb_counts(usb_kb_count, usb_mouse_count, usb_joy_count);
}
#endif

/**
 * HID report event has occurred
 *
 * @param dev_addr    Address of device sending report
 * @param instance    Instance of device sending report
 * @param report      Address of report structure
 * @param len         Length of report structure
 */
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    uint8_t const hid_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch (hid_protocol) {
        case HID_ITF_PROTOCOL_KEYBOARD:
            handle_event_keyboard(dev_addr, instance, (hid_keyboard_report_t const *)report);
            break;

        case HID_ITF_PROTOCOL_MOUSE:
            handle_event_mouse(dev_addr, instance, (hid_mouse_report_t const *)report);
            break;

        default:
            // if report was not immediately identifiable as a keyboard event, read the usage page;
            // some reports have a classifier as "desktop" for media keys, power, or are just encapsulated.
            process_report(dev_addr, instance, report, len);
            break;
    }

    // continue to request to receive report
    tuh_hid_receive_report(dev_addr, instance);
    // if (!tuh_hid_receive_report(dev_addr, instance))
        // ahprintf("[ERROR] unable to receive hid event report\n");
}

/**
 * HID Boot Protocol keeps a six-key buffer of pressed keys. Return true if keycode is "pressed".
 *
 * @param report    Address of hid_keyboard_report_t struct
 * @param keycode   Keycode to look for
 * @return true     Key is currently pressed
 * @return false    Key is not currently pressed
 */
static inline bool key_pressed(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for (uint8_t pos = 0; pos < 6; pos++)
        if (report->keycode[pos] == keycode)
            return true;

    return false;
}

/**
 * Process incoming event and pass off to device-centric handler.
 *
 * @param dev_addr  Address of reporting device
 * @param instance  Instance of reporting device
 * @param report    Address of the report data structure
 * @param len       Size of the report event
 */
static void process_report(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    uint8_t const report_count = hid_info[instance].report_count;
    tuh_hid_report_info_t *report_info_arr = hid_info[instance].report_info;
    tuh_hid_report_info_t *report_info = NULL;

    if (report_count == 1 && report_info_arr[0].report_id == 0) {
        // single report with id of 0 is a single-shot report
        report_info = &report_info_arr[0];
    } else {
        // (possibly, but not mandatory) array of reports
        // @todo i don't understand why, if we're facing an array of reports, why we're only going to process the
        // report containing the identifier at byte 0, but maybe one day that will make sense.
        uint8_t const report_id = report[0];

        // locate the array member matching the report ID we are intending to process (then exit the loop)
        for (uint8_t i = 0; i < report_count; i++) {
            if (report_id == report_info_arr[i].report_id) {
                report_info = &report_info_arr[i];
                break;
            }
        }

        report++;
        len--;
    }

    if (!report_info) {
        // ahprintf("[ERROR] report_info pointer was not set during process_report(), value: $%x, report_count was %d\n", report_info, report_count);
        return;
    }

    // process report event based on usage_page class and usage device
    if (report_info->usage_page == HID_USAGE_PAGE_DESKTOP) {
        switch (report_info->usage) {
            case HID_USAGE_DESKTOP_KEYBOARD:
                // keyboard event; let's hope it appears as a boot proto event or else this will break
                handle_event_keyboard(dev_addr, instance, (hid_keyboard_report_t const *) report);
                break;

            case HID_USAGE_DESKTOP_MOUSE:
                // mouse event
                handle_event_mouse(dev_addr, instance, (hid_mouse_report_t const *) report);
                break;

            case HID_USAGE_DESKTOP_JOYSTICK:
            case HID_USAGE_DESKTOP_GAMEPAD:
                // gamepad/joystick event - map first one to joystick port 2
                if (first_gamepad_dev_addr == 0 || (first_gamepad_dev_addr == dev_addr && first_gamepad_instance == instance)) {
                    // Track first gamepad or update existing one
                    if (first_gamepad_dev_addr == 0) {
                        first_gamepad_dev_addr = dev_addr;
                        first_gamepad_instance = instance;
                    }
                    handle_event_gamepad(dev_addr, instance, report, len);
                }
                break;

            default:
                break;
        }
    }
}

/**
 * Handle the mouse event sent to us.
 *
 * @param dev_addr  Device address of report
 * @param instance  Instance number of reporting device
 * @param report    Address of hid_mouse_report_t structure of current mouse event
 */
static void handle_event_mouse(uint8_t dev_addr, uint8_t instance, hid_mouse_report_t const *report)
{
    static hid_mouse_report_t last_report = { 0 };

    if (report == NULL) {
        // ahprintf("[hid] report was null, aborting mouse event\n");
        return;
    }

    // have buttons changed since the last report?
    if ((report->buttons & MOUSE_BUTTON_LEFT) && !(last_report.buttons & MOUSE_BUTTON_LEFT))
        amiga_quad_mouse_button(AQM_LEFT, true);
    if (!(report->buttons & MOUSE_BUTTON_LEFT) && (last_report.buttons & MOUSE_BUTTON_LEFT))
        amiga_quad_mouse_button(AQM_LEFT, false);

    if ((report->buttons & MOUSE_BUTTON_MIDDLE) && !(last_report.buttons & MOUSE_BUTTON_MIDDLE))
        amiga_quad_mouse_button(AQM_MIDDLE, true);
    if (!(report->buttons & MOUSE_BUTTON_MIDDLE) && (last_report.buttons & MOUSE_BUTTON_MIDDLE))
        amiga_quad_mouse_button(AQM_MIDDLE, false);

    // Check current mode: mouse-only or joystick mode
    bool joystick_mode = amiga_joystick_port1_is_joystick_mode();
    
    if (joystick_mode) {
        // Joystick mode: convert mouse input to joystick signals
        // No mouse quadrature output, only joystick conversion
        amiga_joystick_port1_set_from_mouse(report->x, report->y, report->buttons);
    } else {
        // Mouse-only mode: normal mouse operation
        // Handle mouse buttons
        if ((report->buttons & MOUSE_BUTTON_LEFT) && !(last_report.buttons & MOUSE_BUTTON_LEFT))
            amiga_quad_mouse_button(AQM_LEFT, true);
        if (!(report->buttons & MOUSE_BUTTON_LEFT) && (last_report.buttons & MOUSE_BUTTON_LEFT))
            amiga_quad_mouse_button(AQM_LEFT, false);
        
        if ((report->buttons & MOUSE_BUTTON_MIDDLE) && !(last_report.buttons & MOUSE_BUTTON_MIDDLE))
            amiga_quad_mouse_button(AQM_MIDDLE, true);
        if (!(report->buttons & MOUSE_BUTTON_MIDDLE) && (last_report.buttons & MOUSE_BUTTON_MIDDLE))
            amiga_quad_mouse_button(AQM_MIDDLE, false);
        
        if ((report->buttons & MOUSE_BUTTON_RIGHT) && !(last_report.buttons & MOUSE_BUTTON_RIGHT))
            amiga_quad_mouse_button(AQM_RIGHT, true);
        if (!(report->buttons & MOUSE_BUTTON_RIGHT) && (last_report.buttons & MOUSE_BUTTON_RIGHT))
            amiga_quad_mouse_button(AQM_RIGHT, false);
        
        // Handle mouse movement (quadrature signals)
        // this would spam horrendously, so even when debug messages are on, this is probably... too much.
        // ahprintf("[hid] x: %d y: %d\n", report->x, report->y);
        
        if (report->x || report->y)
            amiga_quad_mouse_set_motion(report->x, report->y);
    }

    last_report = *report;
}

/**
 * Handle gamepad/joystick event and map to joystick port 2
 * This is a basic implementation that attempts to parse common gamepad formats
 * 
 * @param dev_addr  Device address of report
 * @param instance  Instance number of reporting device
 * @param report    Address of gamepad report structure
 * @param len       Length of report
 */
static void handle_event_gamepad(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)dev_addr;  // Unused for now
    (void)instance;  // Unused for now
    
    if (report == NULL || len == 0) {
        return;
    }
    
    // Basic gamepad report parsing - this is a simplified implementation
    // Most gamepads report X/Y for left stick and buttons
    // We'll use a simple heuristic: if the report has X/Y/Button data, parse it
    
    // Common gamepad report format (simplified):
    // Byte 0: Buttons (bitmap)
    // Byte 1-2: X axis (signed, -128 to 127 or 0-255)
    // Byte 2-3: Y axis (signed, -128 to 127 or 0-255)
    // Additional bytes: more buttons, analog triggers, etc.
    
    // For now, use a very basic approach: check if we have at least 3 bytes
    // and try to extract directions from X/Y and buttons
    if (len < 3) {
        return;  // Report too short
    }
    
    // Extract button state (assume first byte is buttons, bit 0 = button 1/fire)
    uint8_t buttons = report[0];
    
    // Extract X/Y axes (assume bytes 1-2 are X and Y, signed)
    // Map 0-255 to -128 to 127 range, or handle as signed directly
    int8_t x = 0, y = 0;
    if (len >= 3) {
        // Common format: X and Y are signed bytes
        x = (int8_t)report[1];
        y = (int8_t)report[2];
    }
    
    // Convert analog stick/D-pad to directions with threshold
    const int8_t deadzone = 10;  // Deadzone to avoid drift
    
    // Horizontal direction
    if (x < -deadzone) {
        // Left
        amiga_joystick_port2_set_direction(AJ2_LEFT, true);
        amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    } else if (x > deadzone) {
        // Right
        amiga_joystick_port2_set_direction(AJ2_LEFT, false);
        amiga_joystick_port2_set_direction(AJ2_RIGHT, true);
    } else {
        // No horizontal
        amiga_joystick_port2_set_direction(AJ2_LEFT, false);
        amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    }
    
    // Vertical direction (note: Y axis is often inverted in gamepads)
    if (y < -deadzone) {
        // Up (Y is inverted in most gamepads)
        amiga_joystick_port2_set_direction(AJ2_UP, true);
        amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    } else if (y > deadzone) {
        // Down
        amiga_joystick_port2_set_direction(AJ2_UP, false);
        amiga_joystick_port2_set_direction(AJ2_DOWN, true);
    } else {
        // No vertical
        amiga_joystick_port2_set_direction(AJ2_UP, false);
        amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    }
    
    // Map buttons (bit 0 = Fire, bit 1 = Button 2, bit 2 = Button 3)
    amiga_joystick_port2_set_button(AJ2_FIRE, (buttons & 0x01) != 0);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, (buttons & 0x02) != 0);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, (buttons & 0x04) != 0);
}

static uint8_t led_report = 0;

/**
 * Handle the keyboard event sent to us.
 *
 * @param dev_addr  Device address of report
 * @param instance  Instance number of reporting device
 * @param report    Address of hid_keyboard_report_t structure of current keyboard event (boot proto?)
 */
static void handle_event_keyboard(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
    // keep hold of older key event reports; init empty keyboard report
    static hid_keyboard_report_t last_report = { 0, 0, {0} };
    static uint32_t call_count = 0;
    uint8_t pos;

    // Debug: Verify handler is being called (print every 100 calls to avoid spam)
    call_count++;
    if ((call_count % 100) == 0) {
        ahprintf("[TOGGLE] USB Keyboard handler called %lu times\n", call_count);
    }
    
    // Debug: Print on first call to verify handler is active
    if (call_count == 1) {
        ahprintf("[TOGGLE] USB Keyboard handler initialized\n");
    }

    // Check for Port 1 mode toggle: Shift + Left Amiga + J
    // This combination toggles between mouse-only and joystick mode on port 1
    static bool last_combo_pressed = false;
    
    // Check for Llamatron mode toggle: Shift + Left Amiga + L
    // This combination toggles Llamatron twinstick mode
    static bool last_llamatron_combo_pressed = false;
    
    bool shift_pressed = (report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
    bool lamiga_pressed = (report->modifier & KEYBOARD_MODIFIER_LEFTGUI) != 0;
    bool j_pressed = false;
    bool l_pressed = false;
    
    // Check if J key is pressed (HID keycode 0x0D = J)
    // Check if L key is pressed (HID keycode 0x0F = L)
    for (pos = 0; pos < 6; pos++) {
        if (report->keycode[pos] == 0x0D) {  // HID keycode for J
            j_pressed = true;
        }
        if (report->keycode[pos] == 0x0F) {  // HID keycode for L
            l_pressed = true;
        }
    }
    
    // Check if all three keys are pressed together for Port 1 toggle
    bool combo_active = shift_pressed && lamiga_pressed && j_pressed;
    
    // Check if all three keys are pressed together for Llamatron toggle
    bool llamatron_combo_active = shift_pressed && lamiga_pressed && l_pressed;
    
    // Debug: Print when any combo key is pressed or when combo is active
    if (j_pressed || shift_pressed || lamiga_pressed || combo_active) {
        ahprintf("[TOGGLE] Shift:%d LAmiga:%d J:%d Combo:%d Modifier:0x%02x Keys:", 
                 shift_pressed, lamiga_pressed, j_pressed, combo_active, report->modifier);
        for (pos = 0; pos < 6; pos++) {
            if (report->keycode[pos] != 0) {
                ahprintf(" 0x%02x", report->keycode[pos]);
            }
        }
        ahprintf("\n");
    }
    
    // Toggle Port 1 mode on key press (when combo is newly pressed, not on release) to avoid multiple toggles
    if (combo_active && !last_combo_pressed) {
        bool old_mode = amiga_joystick_port1_is_joystick_mode();
        amiga_joystick_port1_toggle_mode();
        bool new_mode = amiga_joystick_port1_is_joystick_mode();
        ahprintf("[TOGGLE] *** Port 1 mode toggled! %s -> %s ***\n", 
                 old_mode ? "JOYSTICK" : "MOUSE",
                 new_mode ? "JOYSTICK" : "MOUSE");
        // Don't send the J key to Amiga when used in toggle combination
    }
    
    // Toggle Llamatron mode on key press (when combo is newly pressed, not on release)
    if (llamatron_combo_active && !last_llamatron_combo_pressed) {
        llamatron_mode = !llamatron_mode;
        if (llamatron_mode) {
            // When enabling Llamatron mode, save current state and activate joystick mode on port 1
            llamatron_restore_joystick_mode = amiga_joystick_port1_is_joystick_mode();
            if (!llamatron_restore_joystick_mode) {
                amiga_joystick_port1_toggle_mode();
            }
            ahprintf("[LLAMATRON] *** Llamatron mode ENABLED (was %s mode) ***\n", 
                     llamatron_restore_joystick_mode ? "JOYSTICK" : "MOUSE");
        } else {
            llamatron_active = false;
            // Restore previous port 1 mode
            bool current_mode = amiga_joystick_port1_is_joystick_mode();
            if (current_mode != llamatron_restore_joystick_mode) {
                amiga_joystick_port1_toggle_mode();
            }
            ahprintf("[LLAMATRON] *** Llamatron mode DISABLED (restored %s mode) ***\n",
                     llamatron_restore_joystick_mode ? "JOYSTICK" : "MOUSE");
        }
        // Don't send the L key to Amiga when used in toggle combination
    }
    
    last_combo_pressed = combo_active;
    last_llamatron_combo_pressed = llamatron_combo_active;

    // check to see if a keypress is a new keypress or in the last report
    for (pos = 0; pos < 6; pos++) {
        if (report->keycode[pos] && !key_pressed(&last_report, report->keycode[pos])) {
            // Skip J key if it's part of the Port 1 toggle combination
            if (combo_active && report->keycode[pos] == 0x0D) {  // HID keycode for J
                ahprintf("[TOGGLE] Blocking J key from being sent to Amiga (toggle combo active)\n");
                continue;
            }
            // Skip L key if it's part of the Llamatron toggle combination
            if (llamatron_combo_active && report->keycode[pos] == 0x0F) {  // HID keycode for L
                ahprintf("[LLAMATRON] Blocking L key from being sent to Amiga (toggle combo active)\n");
                continue;
            }
            // this is a new keypress; pass on to the amiga as a down event
            // @todo right now, menu and right gui are both mapped to right amiga; if one is released, an ramiga up is sent
            // probably something which can be fixed in keyboard_serial_io.c
            amiga_hid_send(report->keycode[pos], false);
        }

        if (last_report.keycode[pos] && !key_pressed(report, last_report.keycode[pos])) {
            // Skip J key if it was part of the Port 1 toggle combination
            if (last_combo_pressed && last_report.keycode[pos] == 0x0D) {  // HID keycode for J
                ahprintf("[TOGGLE] Blocking J key release from being sent to Amiga\n");
                continue;
            }
            // Skip L key if it was part of the Llamatron toggle combination
            if (last_llamatron_combo_pressed && last_report.keycode[pos] == 0x0F) {  // HID keycode for L
                ahprintf("[LLAMATRON] Blocking L key release from being sent to Amiga\n");
                continue;
            }
            // key has been released; send "up" code to amiga
            amiga_hid_send(last_report.keycode[pos], true);
        }
    }

    // check modifier state
    _MULTI_MOD_CHECK(KEYBOARD_MODIFIER_LEFTCTRL, KEYBOARD_MODIFIER_RIGHTCTRL);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTALT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTALT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTSHIFT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTSHIFT);
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTGUI);
    // @todo menu key vs right gui thing; see above
    _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTGUI);

    if (amiga_caps_lock()) {
        if (!(led_report & KEYBOARD_LED_CAPSLOCK)) {
            led_report |= KEYBOARD_LED_CAPSLOCK;

            // ahprintf("[hid] turning caps lock led on\n");
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, 1);
        }
    } else {
        if (led_report & KEYBOARD_LED_CAPSLOCK) {
            led_report &= ~KEYBOARD_LED_CAPSLOCK;

            // ahprintf("[hid] turning caps lock led off\n");
            tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, 1);
        }
    }

    last_report = *report;
}

#if ENABLE_BLUEPAD32
// Bluepad32 mouse processing
// Converts uni_mouse_t format and processes it using the existing mouse handling logic
// This allows Bluetooth mice to use the same processing logic as USB mice
// Note: Device count check is now done in process_bluepad32_devices() for efficiency
void process_bluepad32_mouse(void)
{
    // uni_mouse_t structure (matches bluepad32 format)
    typedef struct {
        int32_t delta_x;
        int32_t delta_y;
        uint16_t buttons;
        int8_t scroll_wheel;
        uint8_t misc_buttons;
    } bt_mouse_t;
    
    bt_mouse_t bt_mouse;
    int bt_mouse_count = bluepad32_get_mouse_count();
    
    // Process first connected Bluetooth mouse
    if (bt_mouse_count > 0) {
        bool has_data = bluepad32_get_mouse(0, &bt_mouse);
        
        if (has_data) {
            // Convert Bluepad32 mouse format to HID format
            // Clamp delta_x/delta_y (int32_t) to int8_t range for hid_mouse_report_t
            int8_t x = (bt_mouse.delta_x > 127) ? 127 : (bt_mouse.delta_x < -128) ? -128 : (int8_t)bt_mouse.delta_x;
            int8_t y = (bt_mouse.delta_y > 127) ? 127 : (bt_mouse.delta_y < -128) ? -128 : (int8_t)bt_mouse.delta_y;
            
            // Convert buttons (uint16_t) to uint8_t (take low 8 bits)
            // UNI_MOUSE_BUTTON values match MOUSE_BUTTON values (both use BIT(0), BIT(1), BIT(2))
            uint8_t buttons = (uint8_t)(bt_mouse.buttons & 0xFF);
            
            // Create a temporary mouse report to use existing mouse handling logic
            hid_mouse_report_t mouse_report;
            mouse_report.buttons = buttons;
            mouse_report.x = x;
            mouse_report.y = y;
            
            // Use the existing mouse event handler
            handle_event_mouse(0, 0, &mouse_report);
        }
    }
}

// Bluepad32 keyboard processing
// Converts uni_keyboard_t format to hid_keyboard_report_t format and processes it
// This allows Bluetooth keyboards to use the same processing logic as USB keyboards
// Note: Device count check is now done in process_bluepad32_devices() for efficiency
void process_bluepad32_keyboard(void)
{
    // uni_keyboard_t structure (matches bluepad32 format)
    typedef struct {
        uint8_t modifiers;
        uint8_t pressed_keys[10];  // UNI_KEYBOARD_PRESSED_KEYS_MAX = 10
    } bt_keyboard_t;
    
    bt_keyboard_t bt_kb;
    int bt_kb_count = bluepad32_get_keyboard_count();
    
    // Maximum number of Bluetooth keyboards supported (must match bluepad32_platform.c)
    #define MAX_BT_KEYBOARDS_SUPPORTED 2
    
    // Limit to maximum supported keyboards
    if (bt_kb_count > MAX_BT_KEYBOARDS_SUPPORTED) {
        bt_kb_count = MAX_BT_KEYBOARDS_SUPPORTED;
    }
    
    // Combined state from all keyboards (merged input)
    hid_keyboard_report_t merged_report = { 0, 0, {0} };
    bool merged_report_valid = false;
    
    // Check for Port 1 mode toggle: Shift + Left Amiga + J (only check first keyboard to avoid multiple toggles)
    // This combination toggles between mouse-only and joystick mode on port 1
    static bool bt_last_combo_pressed = false;
    
    // Check for Llamatron mode toggle: Shift + Left Amiga + L (only check first keyboard)
    // This combination toggles Llamatron twinstick mode
    static bool bt_last_llamatron_combo_pressed = false;
    
    bool combo_active = false;
    bool llamatron_combo_active = false;
    
    // Process all connected Bluetooth keyboards
    for (int kb_idx = 0; kb_idx < bt_kb_count; kb_idx++) {
        bool has_data = bluepad32_get_keyboard(kb_idx, &bt_kb);
        
        if (!has_data) {
            continue;
        }
        
        // Check combo keys only from first keyboard (index 0) to avoid multiple toggles
        if (kb_idx == 0) {
            bool shift_pressed = (bt_kb.modifiers & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
            bool lamiga_pressed = (bt_kb.modifiers & KEYBOARD_MODIFIER_LEFTGUI) != 0;
            bool j_pressed = false;
            bool l_pressed = false;
            
            // Check if J key is pressed (HID keycode 0x0D = J)
            for (int i = 0; i < 10; i++) {
                if (bt_kb.pressed_keys[i] == 0x0D) {  // HID keycode for J
                    j_pressed = true;
                }
                if (bt_kb.pressed_keys[i] == 0x0F) {  // HID keycode for L
                    l_pressed = true;
                }
            }
            
            // Check if all three keys are pressed together for Port 1 toggle
            combo_active = shift_pressed && lamiga_pressed && j_pressed;
            
            // Check if all three keys are pressed together for Llamatron toggle
            llamatron_combo_active = shift_pressed && lamiga_pressed && l_pressed;
            
            // Debug: Print when any combo key is pressed or when combo is active
            if (j_pressed || shift_pressed || lamiga_pressed || combo_active) {
                ahprintf("[TOGGLE-BT] Shift:%d LAmiga:%d J:%d Combo:%d Modifier:0x%02x Keys:", 
                         shift_pressed, lamiga_pressed, j_pressed, combo_active, bt_kb.modifiers);
                for (int i = 0; i < 10; i++) {
                    if (bt_kb.pressed_keys[i] != 0) {
                        ahprintf(" 0x%02x", bt_kb.pressed_keys[i]);
                    }
                }
                ahprintf("\n");
            }
            
            // Toggle Port 1 mode on key press (when combo is newly pressed)
            if (combo_active && !bt_last_combo_pressed) {
                bool old_mode = amiga_joystick_port1_is_joystick_mode();
                amiga_joystick_port1_toggle_mode();
                bool new_mode = amiga_joystick_port1_is_joystick_mode();
                ahprintf("[TOGGLE-BT] *** Port 1 mode toggled! %s -> %s ***\n", 
                         old_mode ? "JOYSTICK" : "MOUSE",
                         new_mode ? "JOYSTICK" : "MOUSE");
            }
            
            // Toggle Llamatron mode on key press (when combo is newly pressed)
            if (llamatron_combo_active && !bt_last_llamatron_combo_pressed) {
                llamatron_mode = !llamatron_mode;
                if (llamatron_mode) {
                    // When enabling Llamatron mode, save current state and activate joystick mode on port 1
                    llamatron_restore_joystick_mode = amiga_joystick_port1_is_joystick_mode();
                    if (!llamatron_restore_joystick_mode) {
                        amiga_joystick_port1_toggle_mode();
                    }
                    ahprintf("[LLAMATRON-BT] *** Llamatron mode ENABLED (was %s mode) ***\n", 
                             llamatron_restore_joystick_mode ? "JOYSTICK" : "MOUSE");
                } else {
                    llamatron_active = false;
                    // Restore previous port 1 mode
                    bool current_mode = amiga_joystick_port1_is_joystick_mode();
                    if (current_mode != llamatron_restore_joystick_mode) {
                        amiga_joystick_port1_toggle_mode();
                    }
                    ahprintf("[LLAMATRON-BT] *** Llamatron mode DISABLED (restored %s mode) ***\n",
                             llamatron_restore_joystick_mode ? "JOYSTICK" : "MOUSE");
                }
            }
            
            bt_last_combo_pressed = combo_active;
            bt_last_llamatron_combo_pressed = llamatron_combo_active;
        }
        
        // Convert Bluepad32 keyboard format to HID format for this keyboard
        hid_keyboard_report_t kb_report;
        kb_report.modifier = bt_kb.modifiers;
        kb_report.reserved = 0;
        
        // Copy pressed keys (up to 6 keys for HID standard)
        // Skip J key if it's being used for Port 1 toggle (only from first keyboard)
        // Skip L key if it's being used for Llamatron toggle (only from first keyboard)
        int key_count = 0;
        for (int i = 0; i < 10 && key_count < 6; i++) {
            if (bt_kb.pressed_keys[i] != 0) {
                // Skip J key (HID 0x0D) if it's being used for Port 1 toggle (only from first keyboard)
                if (kb_idx == 0 && combo_active && bt_kb.pressed_keys[i] == 0x0D) {
                    ahprintf("[TOGGLE-BT] Blocking J key from being sent to Amiga\n");
                    continue;
                }
                // Skip L key (HID 0x0F) if it's being used for Llamatron toggle (only from first keyboard)
                if (kb_idx == 0 && llamatron_combo_active && bt_kb.pressed_keys[i] == 0x0F) {
                    ahprintf("[LLAMATRON-BT] Blocking L key from being sent to Amiga\n");
                    continue;
                }
                kb_report.keycode[key_count++] = bt_kb.pressed_keys[i];
            }
        }
        // Zero out remaining slots
        for (int i = key_count; i < 6; i++) {
            kb_report.keycode[i] = 0;
        }
        
        // Merge modifiers: OR logic (if any keyboard has modifier pressed, set it)
        merged_report.modifier |= kb_report.modifier;
        
        // Merge pressed keys: Union (add all unique keys from all keyboards)
        // Track which keys we've already added to merged report
        for (int i = 0; i < 6; i++) {
            if (kb_report.keycode[i] != 0) {
                // Check if this key is already in merged report
                bool key_already_present = false;
                for (int j = 0; j < 6; j++) {
                    if (merged_report.keycode[j] == kb_report.keycode[i]) {
                        key_already_present = true;
                        break;
                    }
                }
                
                // Add key if not already present and we have room
                if (!key_already_present) {
                    for (int j = 0; j < 6; j++) {
                        if (merged_report.keycode[j] == 0) {
                            merged_report.keycode[j] = kb_report.keycode[i];
                            break;
                        }
                    }
                }
            }
        }
        
        merged_report_valid = true;
    }
    
    // Process merged keyboard input from all keyboards
    if (merged_report_valid) {
        // Use combined last report for merged state tracking
        static hid_keyboard_report_t last_merged_report = { 0, 0, {0} };
        uint8_t pos;
        
        // Check for new keypresses or releases across all keyboards
        for (pos = 0; pos < 6; pos++) {
            if (merged_report.keycode[pos] && !key_pressed(&last_merged_report, merged_report.keycode[pos])) {
                // New keypress (from any keyboard)
                amiga_hid_send(merged_report.keycode[pos], false);
            }
            
            if (last_merged_report.keycode[pos] && !key_pressed(&merged_report, last_merged_report.keycode[pos])) {
                // Key released (no longer pressed on any keyboard)
                amiga_hid_send(last_merged_report.keycode[pos], true);
            }
        }
        
        // Check modifier state (macros expect 'report' and 'last_report' in scope)
        hid_keyboard_report_t* report = &merged_report;
        hid_keyboard_report_t last_report = last_merged_report;
        _MULTI_MOD_CHECK(KEYBOARD_MODIFIER_LEFTCTRL, KEYBOARD_MODIFIER_RIGHTCTRL);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTALT);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTALT);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTSHIFT);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTSHIFT);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_LEFTGUI);
        _SINGLE_MOD_CHECK(KEYBOARD_MODIFIER_RIGHTGUI);
        
        last_merged_report = merged_report;
    }
}

// Bluepad32 gamepad processing
// Converts uni_gamepad_t format and maps gamepads to joystick ports:
// - First gamepad (index 0) -> Joystick Port 2 (always active)
// - Second gamepad (index 1) -> Joystick Port 1 (only if Port 1 is in joystick mode)
void process_bluepad32_gamepad(void)
{
    // uni_gamepad_t structure (matches bluepad32 format exactly)
    // Note: We can't include uni.h directly due to HID type conflicts with TinyUSB
    // IMPORTANT: This struct MUST match uni_gamepad_t exactly, including gyro and accel fields
    // to prevent memory corruption during struct copy operations
    typedef struct {
        uint8_t dpad;          // D-pad bitmap (DPAD_UP=1, DPAD_DOWN=2, DPAD_RIGHT=4, DPAD_LEFT=8)
        int32_t axis_x;        // Left stick X (-512 to 511)
        int32_t axis_y;        // Left stick Y (-512 to 511)
        int32_t axis_rx;       // Right stick X (-512 to 511)
        int32_t axis_ry;       // Right stick Y (-512 to 511)
        int32_t brake;         // Brake/trigger
        int32_t throttle;      // Throttle/trigger
        uint16_t buttons;      // Button bitmap (BUTTON_A=1, BUTTON_B=2, BUTTON_X=4, BUTTON_Y=8)
        uint8_t misc_buttons;  // Misc buttons
        int32_t gyro[3];       // Gyroscope data (degrees/second) - REQUIRED for correct struct size
        int32_t accel[3];      // Accelerometer data (G units) - REQUIRED for correct struct size
    } bt_gamepad_t;
    
    bt_gamepad_t bt_gamepad;
    int bt_gamepad_count = bluepad32_get_gamepad_count();
    
    // Analog stick calibration constants (shared for both ports)
    const int32_t ANALOG_STICK_DEADZONE = 80;  // ~16% of -512 range (matches Atari implementation)
    
    // Helper macro to convert gamepad input to direction bits
    // Returns: bit 0=UP, bit 1=DOWN, bit 2=LEFT, bit 3=RIGHT
    #define CONVERT_GAMEPAD_TO_DIRECTIONS(gp, deadzone) ({ \
        uint8_t dir_bits = 0; \
        if ((gp)->dpad & 0x01) { dir_bits |= 0x01; }  /* UP */ \
        if ((gp)->dpad & 0x02) { dir_bits |= 0x02; }  /* DOWN */ \
        if ((gp)->dpad & 0x04) { dir_bits |= 0x08; }  /* RIGHT */ \
        if ((gp)->dpad & 0x08) { dir_bits |= 0x04; }  /* LEFT */ \
        if (dir_bits == 0) { \
            if ((gp)->axis_x < -(deadzone)) dir_bits |= 0x04;  /* LEFT */ \
            if ((gp)->axis_x > (deadzone))  dir_bits |= 0x08;  /* RIGHT */ \
            if ((gp)->axis_y < -(deadzone)) dir_bits |= 0x01;  /* UP */ \
            if ((gp)->axis_y > (deadzone))  dir_bits |= 0x02;  /* DOWN */ \
        } \
        dir_bits; \
    })
    
    // Check if Llamatron mode is active (requires exactly one gamepad and port 1 in joystick mode)
    if (llamatron_mode && bt_gamepad_count == 1 && amiga_joystick_port1_is_joystick_mode()) {
        bool has_data = bluepad32_get_gamepad(0, &bt_gamepad);
        
        if (has_data) {
            llamatron_active = true;
            
            // Helper macro to convert right stick to direction bits
            #define CONVERT_RIGHT_STICK_TO_DIRECTIONS(gp, deadzone) ({ \
                uint8_t dir_bits = 0; \
                int32_t rx = (gp)->axis_rx; \
                int32_t ry = (gp)->axis_ry; \
                if (rx < -(deadzone)) dir_bits |= 0x04;  /* LEFT */ \
                if (rx > (deadzone))  dir_bits |= 0x08;  /* RIGHT */ \
                if (ry < -(deadzone)) dir_bits |= 0x01;  /* UP */ \
                if (ry > (deadzone))  dir_bits |= 0x02;  /* DOWN */ \
                dir_bits; \
            })
            
            // Port 2: Left stick (axis_x, axis_y) - Movement (D-pad has priority)
            uint8_t port2_direction_bits = CONVERT_GAMEPAD_TO_DIRECTIONS(&bt_gamepad, ANALOG_STICK_DEADZONE);
            amiga_joystick_port2_set_direction(AJ2_UP,    (port2_direction_bits & 0x01) != 0);
            amiga_joystick_port2_set_direction(AJ2_DOWN,  (port2_direction_bits & 0x02) != 0);
            amiga_joystick_port2_set_direction(AJ2_LEFT,  (port2_direction_bits & 0x04) != 0);
            amiga_joystick_port2_set_direction(AJ2_RIGHT, (port2_direction_bits & 0x08) != 0);
            
            // Port 2 fire: BUTTON_B (movement stick fire button)
            amiga_joystick_port2_set_button(AJ2_FIRE, (bt_gamepad.buttons & 0x02) != 0);  // BUTTON_B
            
            // Port 1: Right stick (axis_rx, axis_ry) - Fire direction
            uint8_t port1_direction_bits = CONVERT_RIGHT_STICK_TO_DIRECTIONS(&bt_gamepad, ANALOG_STICK_DEADZONE);
            amiga_joystick_port1_set_direction(AJ1_UP,    (port1_direction_bits & 0x01) != 0);
            amiga_joystick_port1_set_direction(AJ1_DOWN,  (port1_direction_bits & 0x02) != 0);
            amiga_joystick_port1_set_direction(AJ1_LEFT,  (port1_direction_bits & 0x04) != 0);
            amiga_joystick_port1_set_direction(AJ1_RIGHT, (port1_direction_bits & 0x08) != 0);
            
            // Port 1 fire: BUTTON_A (fire direction stick fire button)
            amiga_joystick_port1_set_button(AJ1_FIRE, (bt_gamepad.buttons & 0x01) != 0);  // BUTTON_A
            
            #undef CONVERT_RIGHT_STICK_TO_DIRECTIONS
        } else {
            llamatron_active = false;
        }
    } else {
        llamatron_active = false;
        
        // Normal mode: Process first gamepad -> Joystick Port 2 (always active)
        if (bt_gamepad_count > 0) {
            bool has_data = bluepad32_get_gamepad(0, &bt_gamepad);
            
            if (has_data) {
                uint8_t direction_bits = CONVERT_GAMEPAD_TO_DIRECTIONS(&bt_gamepad, ANALOG_STICK_DEADZONE);
                
                // Map direction bits to joystick port 2
                // Bit pattern: bit 0=UP, bit 1=DOWN, bit 2=LEFT, bit 3=RIGHT
                amiga_joystick_port2_set_direction(AJ2_UP,    (direction_bits & 0x01) != 0);
                amiga_joystick_port2_set_direction(AJ2_DOWN,  (direction_bits & 0x02) != 0);
                amiga_joystick_port2_set_direction(AJ2_LEFT,  (direction_bits & 0x04) != 0);
                amiga_joystick_port2_set_direction(AJ2_RIGHT, (direction_bits & 0x08) != 0);
                
                // Map buttons using Bluepad32 button constants
                // BUTTON_A = BIT(0) = 1 (Fire)
                // BUTTON_B = BIT(1) = 2 (Button 2)
                // BUTTON_X = BIT(2) = 4 (Button 3)
                // BUTTON_Y = BIT(3) = 8 (Button 3 alternative)
                amiga_joystick_port2_set_button(AJ2_FIRE, (bt_gamepad.buttons & 0x01) != 0);      // BUTTON_A
                amiga_joystick_port2_set_button(AJ2_BUTTON2, (bt_gamepad.buttons & 0x02) != 0);  // BUTTON_B
                // Use X or Y for Button 3
                amiga_joystick_port2_set_button(AJ2_BUTTON3, (bt_gamepad.buttons & 0x04) != 0 || (bt_gamepad.buttons & 0x08) != 0);  // BUTTON_X or BUTTON_Y
            }
        }
        
        // Process second gamepad -> Joystick Port 1 (only if Port 1 is in joystick mode)
        if (bt_gamepad_count > 1 && amiga_joystick_port1_is_joystick_mode()) {
            bool has_data = bluepad32_get_gamepad(1, &bt_gamepad);
            
            if (has_data) {
                uint8_t direction_bits = CONVERT_GAMEPAD_TO_DIRECTIONS(&bt_gamepad, ANALOG_STICK_DEADZONE);
                
                // Map direction bits to joystick port 1
                // Bit pattern: bit 0=UP, bit 1=DOWN, bit 2=LEFT, bit 3=RIGHT
                amiga_joystick_port1_set_direction(AJ1_UP,    (direction_bits & 0x01) != 0);
                amiga_joystick_port1_set_direction(AJ1_DOWN,  (direction_bits & 0x02) != 0);
                amiga_joystick_port1_set_direction(AJ1_LEFT,  (direction_bits & 0x04) != 0);
                amiga_joystick_port1_set_direction(AJ1_RIGHT, (direction_bits & 0x08) != 0);
                
                // Map buttons using Bluepad32 button constants
                amiga_joystick_port1_set_button(AJ1_FIRE, (bt_gamepad.buttons & 0x01) != 0);      // BUTTON_A
                amiga_joystick_port1_set_button(AJ1_BUTTON2, (bt_gamepad.buttons & 0x02) != 0);  // BUTTON_B
                amiga_joystick_port1_set_button(AJ1_BUTTON3, (bt_gamepad.buttons & 0x04) != 0 || (bt_gamepad.buttons & 0x08) != 0);  // BUTTON_X or BUTTON_Y
            }
        }
    }
    
    #undef CONVERT_GAMEPAD_TO_DIRECTIONS
}

// Optimized batched Bluepad32 processing
// Checks device counts first and only processes connected devices
// This reduces overhead when no Bluetooth devices are connected
// Placed after all individual processing functions to avoid forward declaration issues
void process_bluepad32_devices(void)
{
    // Early return if no devices are connected (avoids function call overhead)
    int kb_count = bluepad32_get_keyboard_count();
    int mouse_count = bluepad32_get_mouse_count();
    int gamepad_count = bluepad32_get_gamepad_count();
    
    // Only process devices that are actually connected
    if (kb_count > 0) {
        process_bluepad32_keyboard();
    }
    
    if (mouse_count > 0) {
        process_bluepad32_mouse();
    }
    
    if (gamepad_count > 0) {
        process_bluepad32_gamepad();
    }
}
#else
void process_bluepad32_keyboard(void)
{
    // No-op when bluepad32 is disabled
}

void process_bluepad32_mouse(void)
{
    // No-op when bluepad32 is disabled
}

void process_bluepad32_gamepad(void)
{
    // No-op when bluepad32 is disabled
}

void process_bluepad32_devices(void)
{
    // No-op when bluepad32 is disabled
}
#endif // ENABLE_BLUEPAD32
