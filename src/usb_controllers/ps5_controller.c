/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * PS5 DualSense controller implementation (USB HID).
 * Report format: ID 0x31, 78 bytes; payload at offset 2 (see Bluepad32 uni_hid_parser_ds5).
 */

#include "ps5_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include "usb_device_map.h"
#include <stdio.h>
#include <string.h>

//--------------------------------------------------------------------
// Controller Storage
//--------------------------------------------------------------------

#define MAX_PS5_CONTROLLERS  2

static ps5_controller_t controllers[MAX_PS5_CONTROLLERS];
static uint8_t controller_count = 0;

//--------------------------------------------------------------------
// Helper Functions
//--------------------------------------------------------------------

static ps5_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected) {
            return &controllers[i];
        }
    }
    return NULL;
}

static ps5_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_PS5_CONTROLLERS) {
        printf("PS5: Max controllers reached\n");
        return NULL;
    }

    ps5_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(ps5_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    ctrl->deadzone = 20;

    return ctrl;
}

static void free_controller(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr) {
            for (uint8_t j = i; j < controller_count - 1; j++) {
                controllers[j] = controllers[j + 1];
            }
            controller_count--;
            break;
        }
    }
}

//--------------------------------------------------------------------
// Public API Implementation
//--------------------------------------------------------------------

bool ps5_is_dualsense(uint16_t vid, uint16_t pid) {
    if (vid != PS5_VENDOR_ID) {
        return false;
    }
    return (pid == PS5_DUALENSE_PID || pid == PS5_DUALENSE_EDGE_PID);
}

// DualSense USB uses report ID 0x01, 64 bytes. Layout matches joypad-os sony_ds5_report_t:
// payload[0..6] = x1, y1, x2, y2, rx, ry, rz (sticks/triggers); payload[7..8] = button bytes.
// HID convention for Y: 0=up, 255=down (joypad-os sony_ds5.c).
#define PS5_USB_REPORT_ID  0x01
#define PS5_USB_MIN_LEN   10   // 1 (ID) + 9 (x1,y1,x2,y2,rx,ry,rz, buttons0, buttons1)

static void ps5_parse_usb_report(ps5_report_mini_t* input, const uint8_t* payload) {
    input->x     = payload[0];   // x1 left stick X
    input->y     = payload[1];   // y1 left stick Y
    input->rx    = payload[2];   // x2 right stick X
    input->ry    = payload[3];   // y2 right stick Y
    input->brake = payload[4];   // rx (or L2 analog if present)
    input->throttle = payload[5]; // ry (or R2 analog if present)
    input->buttons[0] = payload[7]; // dpad:4, square, cross, circle, triangle
    input->buttons[1] = payload[8]; // L1, R1, L2, R2, share, option, L3, R3
}

static void ps5_parse_bt_report(ps5_report_mini_t* input, const uint8_t* payload, uint16_t payload_len) {
    input->x = payload[0];
    input->y = payload[1];
    input->rx = payload[2];
    input->ry = payload[3];
    input->brake = payload[4];
    input->throttle = payload[5];
    input->seq_number = payload[6];
    input->buttons[0] = payload[7];
    input->buttons[1] = payload[8];
    if (payload_len >= 13) {
        input->buttons[2] = payload[11];
        input->buttons[3] = payload[12];
    }
}

bool ps5_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    ps5_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) {
            return false;
        }
    }

    ps5_report_mini_t* input = &ctrl->report;

    if (report[0] == PS5_USB_REPORT_ID && len >= PS5_USB_MIN_LEN) {
        ps5_parse_usb_report(input, report + 1);
    } else if (report[0] == PS5_REPORT_ID && len >= 2 + 9) {
        ps5_parse_bt_report(input, report + 2, len - 2);
    } else {
        return false;
    }

    ps5_update_amiga_joystick(dev_addr);
    return true;
}

ps5_controller_t* ps5_get_controller(uint8_t dev_addr) {
    return find_controller_by_addr(dev_addr);
}

void ps5_update_amiga_joystick(uint8_t dev_addr) {
    ps5_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        return;
    }

    const ps5_report_mini_t* input = &ctrl->report;

    uint8_t direction = 0;
    uint8_t dpad = input->buttons[0] & 0x0F;

    if (dpad != PS5_DPAD_CENTER && dpad < 8) {
        switch (dpad) {
            case PS5_DPAD_UP:         direction = 0x01; break;
            case PS5_DPAD_UP_RIGHT:   direction = 0x09; break;
            case PS5_DPAD_RIGHT:     direction = 0x08; break;
            case PS5_DPAD_DOWN_RIGHT: direction = 0x0A; break;
            case PS5_DPAD_DOWN:      direction = 0x02; break;
            case PS5_DPAD_DOWN_LEFT:  direction = 0x06; break;
            case PS5_DPAD_LEFT:      direction = 0x04; break;
            case PS5_DPAD_UP_LEFT:   direction = 0x05; break;
            default: break;
        }
    } else {
        int16_t stick_x = (int16_t)input->x - 127;
        int16_t stick_y = (int16_t)input->y - 127;
        if (stick_x < -ctrl->deadzone || stick_x > ctrl->deadzone ||
            stick_y < -ctrl->deadzone || stick_y > ctrl->deadzone) {
            if (stick_y < -ctrl->deadzone) direction |= 0x01;
            if (stick_y > ctrl->deadzone)  direction |= 0x02;
            if (stick_x < -ctrl->deadzone) direction |= 0x04;
            if (stick_x > ctrl->deadzone)  direction |= 0x08;
        }
    }

    amiga_joystick_port2_set_direction(AJ2_UP,    (direction & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN,  (direction & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT,  (direction & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT,  (direction & 0x08) != 0);

    bool cross  = (input->buttons[0] & 0x20) != 0;
    bool circle = (input->buttons[0] & 0x40) != 0;
    bool square = (input->buttons[0] & 0x10) != 0;
    bool r2_digital = (input->buttons[1] & 0x08) != 0; // R2 bit in joypad-os layout
    bool fire = cross || (input->throttle > 200) || r2_digital;
    bool button2 = circle;
    bool button3 = square;

    amiga_joystick_port2_set_button(AJ2_FIRE, fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, button2);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, button3);
}

void ps5_mount_cb(uint8_t dev_addr) {
    printf("PS5: DualSense controller detected (addr=%d)\n", dev_addr);

#if HIDPICO_REVISION == 5
    display_show_controller_detected("PS5", "DualSense", 3000);
#endif

    ps5_controller_t* ctrl = allocate_controller(dev_addr);
    if (ctrl) {
        printf("PS5: Controller registered\n");
        usb_map_register_gamepad(dev_addr, "PS5");
    }
}

void ps5_unmount_cb(uint8_t dev_addr) {
    printf("PS5: Controller unmounted at address %d\n", dev_addr);
    usb_map_unregister_gamepad(dev_addr);
    free_controller(dev_addr);

    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}

uint8_t ps5_connected_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].connected) {
            count++;
        }
    }
    return count;
}
