/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Sony PlayStation Classic controller (0x054C / 0x0CDA).
 * Report format: 3 bytes (joypad-os sony_psc_report_t) - buttons, dpad+buttons, counter.
 */

#include "psc_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include "usb_device_map.h"
#include <stdio.h>
#include <string.h>

#define MAX_PSC_CONTROLLERS  2

typedef struct {
    uint8_t dev_addr;
    bool connected;
    uint8_t dpad;       // HID hat 0-7, 8=released
    uint8_t cross;
    uint8_t circle;
    uint8_t square;
    uint8_t triangle;
    uint8_t l1, r1, l2, r2;
} psc_controller_t;

static psc_controller_t controllers[MAX_PSC_CONTROLLERS];
static uint8_t controller_count = 0;

static psc_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected)
            return &controllers[i];
    }
    return NULL;
}

static psc_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_PSC_CONTROLLERS) {
        printf("PSC: Max controllers reached\n");
        return NULL;
    }
    psc_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(psc_controller_t));
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

bool psc_is_controller(uint16_t vid, uint16_t pid) {
    return (vid == PSC_VENDOR_ID && pid == PSC_PID);
}

// PSC report: byte0 = triangle,circle,cross,square,l2,r2,l1,r1 (bit0=r1, bit7=triangle)
//             byte1 = share,option, d-pad(4 bits)
//             byte2 = counter
void psc_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    if (!report || len < 3) return;
    // Skip report ID if present (some devices send 1-byte prefix)
    if (len >= 4 && (report[0] == 0x00 || report[0] == 0x01)) {
        report++;
        len--;
    }
    if (len < 3) return;

    psc_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) return;
    }

    uint8_t b0 = report[0];
    uint8_t b1 = report[1];
    // Byte 0: triangle:7, circle:6, cross:5, square:4, l2:3, r2:2, l1:1, r1:0 (joypad-os sony_psc_report_t)
    ctrl->triangle = (b0 >> 7) & 1;
    ctrl->circle   = (b0 >> 6) & 1;
    ctrl->cross    = (b0 >> 5) & 1;
    ctrl->square   = (b0 >> 4) & 1;
    ctrl->l2 = (b0 >> 3) & 1;
    ctrl->r2 = (b0 >> 2) & 1;
    ctrl->l1 = (b0 >> 1) & 1;
    ctrl->r1 = (b0 >> 0) & 1;
    ctrl->dpad = b1 & 0x0F;  // 0-7 = direction, 8 = released (if device sends it)

    psc_update_amiga_joystick(dev_addr);
}

void psc_update_amiga_joystick(uint8_t dev_addr) {
    psc_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) return;

    uint8_t direction = 0;
    if (ctrl->dpad < 8) {
        switch (ctrl->dpad) {
            case 0: direction = 0x01; break;  // up
            case 1: direction = 0x09; break;
            case 2: direction = 0x08; break;  // right
            case 3: direction = 0x0A; break;
            case 4: direction = 0x02; break;  // down
            case 5: direction = 0x06; break;
            case 6: direction = 0x04; break;  // left
            case 7: direction = 0x05; break;
            default: break;
        }
    }

    amiga_joystick_port2_set_direction(AJ2_UP,    (direction & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN,  (direction & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT,  (direction & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT,  (direction & 0x08) != 0);

    bool fire = ctrl->cross || ctrl->r2;
    amiga_joystick_port2_set_button(AJ2_FIRE,    fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, ctrl->circle);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, ctrl->square);
}

void psc_mount_cb(uint8_t dev_addr) {
    printf("PSC: PlayStation Classic controller detected (addr=%d)\n", dev_addr);
#if HIDPICO_REVISION == 5
    display_show_controller_detected("PSC", "PlayStation Classic", 3000);
#endif
    if (!allocate_controller(dev_addr))
        printf("PSC: Failed to allocate controller\n");
    else
        usb_map_register_gamepad(dev_addr, "PSC");
}

void psc_unmount_cb(uint8_t dev_addr) {
    printf("PSC: Controller unmount (addr=%d)\n", dev_addr);
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

uint8_t psc_connected_count(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < controller_count; i++)
        if (controllers[i].connected) n++;
    return n;
}
