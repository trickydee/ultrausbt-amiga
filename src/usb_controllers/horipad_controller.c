/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * HORI HORIPAD for Nintendo Switch. Report: byte0 = y,b,a,x,l1,r1,l2,r2;
 * byte1 = s1,s2,l3,r3,a1,a2; byte2 = dpad:4; bytes 3-6 = axis_x,y,z,rz.
 * HID convention: 0=up/left, 128=center, 255=down/right.
 */

#include "horipad_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include "usb_device_map.h"
#include <stdio.h>
#include <string.h>

#define MAX_HORIPAD_CONTROLLERS  2
#define HORIPAD_DEADZONE         20

typedef struct {
    uint8_t dev_addr;
    bool connected;
    uint8_t dpad;
    uint8_t axis_x, axis_y, axis_z, axis_rz;
    uint8_t b, a, y, x, l1, r1, l2, r2;
} horipad_controller_t;

static horipad_controller_t controllers[MAX_HORIPAD_CONTROLLERS];
static uint8_t controller_count = 0;

static horipad_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected)
            return &controllers[i];
    }
    return NULL;
}

static horipad_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_HORIPAD_CONTROLLERS) {
        printf("HORIPAD: Max controllers reached\n");
        return NULL;
    }
    horipad_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(horipad_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    return ctrl;
}

static void free_controller(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr) {
            for (uint8_t j = i; j < controller_count - 1; j++)
                controllers[j] = controllers[j + 1];
            controller_count--;
            break;
        }
    }
}

bool horipad_is_controller(uint16_t vid, uint16_t pid) {
    return (vid == HORIPAD_VENDOR_ID && pid == HORIPAD_PID);
}

// Report layout (joypad-os hori_horipad_report_t): 3 bytes buttons+dpad, 4 bytes axes
void horipad_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    if (!report || len < 7) return;
    if (len >= 8 && (report[0] == 0x00 || report[0] == 0x01)) {
        report++;
        len--;
    }
    if (len < 7) return;

    horipad_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) return;
    }

    uint8_t b0 = report[0], b2 = report[2];
    ctrl->y  = (b0 >> 0) & 1;
    ctrl->b  = (b0 >> 1) & 1;
    ctrl->a  = (b0 >> 2) & 1;
    ctrl->x  = (b0 >> 3) & 1;
    ctrl->l1 = (b0 >> 4) & 1;
    ctrl->r1 = (b0 >> 5) & 1;
    ctrl->l2 = (b0 >> 6) & 1;
    ctrl->r2 = (b0 >> 7) & 1;
    ctrl->dpad = b2 & 0x0F;
    ctrl->axis_x  = report[3];
    ctrl->axis_y  = report[4];
    ctrl->axis_z  = report[5];
    ctrl->axis_rz = report[6];

    horipad_update_amiga_joystick(dev_addr);
}

void horipad_update_amiga_joystick(uint8_t dev_addr) {
    horipad_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) return;

    uint8_t direction = 0;
    if (ctrl->dpad < 8) {
        switch (ctrl->dpad) {
            case 0: direction = 0x01; break;
            case 1: direction = 0x09; break;
            case 2: direction = 0x08; break;
            case 3: direction = 0x0A; break;
            case 4: direction = 0x02; break;
            case 5: direction = 0x06; break;
            case 6: direction = 0x04; break;
            case 7: direction = 0x05; break;
            default: break;
        }
    } else {
        int16_t sx = (int16_t)ctrl->axis_x - 128;
        int16_t sy = (int16_t)ctrl->axis_y - 128;
        if (sy < -HORIPAD_DEADZONE) direction |= 0x01;
        if (sy > HORIPAD_DEADZONE)  direction |= 0x02;
        if (sx < -HORIPAD_DEADZONE) direction |= 0x04;
        if (sx > HORIPAD_DEADZONE)  direction |= 0x08;
    }

    amiga_joystick_port2_set_direction(AJ2_UP,    (direction & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN,  (direction & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT,  (direction & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT,  (direction & 0x08) != 0);

    bool fire = ctrl->b || ctrl->r2;
    amiga_joystick_port2_set_button(AJ2_FIRE,    fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, ctrl->a);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, ctrl->y);
}

void horipad_mount_cb(uint8_t dev_addr) {
    printf("HORIPAD: HORI HORIPAD (Switch) detected (addr=%d)\n", dev_addr);
#if HIDPICO_REVISION == 5
    display_show_controller_detected("HORI", "HORIPAD (Switch)", 3000);
#endif
    if (!allocate_controller(dev_addr))
        printf("HORIPAD: Failed to allocate controller\n");
    else
        usb_map_register_gamepad(dev_addr, "HORIPAD");
}

void horipad_unmount_cb(uint8_t dev_addr) {
    printf("HORIPAD: Controller unmount (addr=%d)\n", dev_addr);
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

uint8_t horipad_connected_count(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < controller_count; i++)
        if (controllers[i].connected) n++;
    return n;
}
