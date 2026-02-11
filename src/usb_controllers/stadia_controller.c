/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Google Stadia controller implementation
 * Based on Atari IKBD implementation
 */

#include "stadia_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STADIA_CONTROLLERS  2

static stadia_controller_t controllers[MAX_STADIA_CONTROLLERS];
static uint8_t controller_count = 0;

static stadia_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected) {
            return &controllers[i];
        }
    }
    return NULL;
}

static stadia_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_STADIA_CONTROLLERS) {
        printf("Stadia: Max controllers reached\n");
        return NULL;
    }
    stadia_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(stadia_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    ctrl->deadzone = 16;  // Match Atari HidInput.cpp DEAD_ZONE (0x10) for same stick feel
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

bool stadia_is_controller(uint16_t vid, uint16_t pid) {
    return (vid == STADIA_VENDOR_ID && pid == STADIA_CONTROLLER_PID);
}

void stadia_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    if (!report || len == 0) return;

    stadia_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) return;
    }

    const uint8_t* d;
    uint8_t offset = 0;

    // Format 1: stadia-vigem (Atari HidInput.cpp) - report ID 0x03, 10-byte payload
    // Byte 0: 0x03 header, 1: D-Pad, 2: System, 3: Face (bit6=A,5=B,4=X,3=Y,2=LB,1=RB,0=LS), 4-5: LX,LY, 6-7: RX,RY, 8-9: LT,RT
    if (len >= 11 && report[0] == 0x03) {
        d = report + 1;
        ctrl->dpad = d[0];
        if (ctrl->dpad >= 8) ctrl->dpad = STADIA_DPAD_NEUTRAL;  // 8 = HID hat released, use stick
        // Face buttons byte: bit 6=A, 5=B, 4=X, 3=Y, 2=LB, 1=RB, 0=LS (L3) -> map to STADIA_BTN_* bits
        ctrl->buttons = ((d[2] >> 6) & 1) | ((d[2] >> 5) & 1) << 1 | ((d[2] >> 4) & 1) << 2
                     | ((d[2] >> 3) & 1) << 3 | ((d[2] >> 2) & 1) << 4 | ((d[2] >> 1) & 1) << 5
                     | (d[2] & 1) << 10;  // LS -> L3 (STADIA_BTN_L3)
        ctrl->stick_left_x = (int16_t)d[3] - 128;
        ctrl->stick_left_y = (int16_t)d[4] - 128;   // Y: up = low value on Stadia
        ctrl->stick_right_x = (int16_t)d[5] - 128;
        ctrl->stick_right_y = (int16_t)d[6] - 128;
        ctrl->trigger_left = d[7];
        ctrl->trigger_right = d[8];
        stadia_update_amiga_joystick(dev_addr);
        return;
    }

    // Format 2: report ID 1-15, 9-byte payload (buttons 16-bit, dpad, sticks, triggers)
    if (len >= 10 && report[0] >= 1 && report[0] <= 15) {
        offset = 1;
    }
    if (len < offset + 9) return;

    d = report + offset;
    ctrl->buttons = d[0] | (d[1] << 8);
    ctrl->dpad = d[2];
    if (ctrl->dpad >= 8) ctrl->dpad = STADIA_DPAD_NEUTRAL;  // 8 = HID hat released, use stick

    ctrl->stick_left_x = (int16_t)d[3] - 128;
    ctrl->stick_left_y = (int16_t)d[4] - 128;   // Y: up = low value on Stadia
    ctrl->stick_right_x = (int16_t)d[5] - 128;
    ctrl->stick_right_y = (int16_t)d[6] - 128;
    ctrl->trigger_left = d[7];
    ctrl->trigger_right = d[8];

    stadia_update_amiga_joystick(dev_addr);
}

stadia_controller_t* stadia_get_controller(uint8_t dev_addr) {
    return find_controller_by_addr(dev_addr);
}

void stadia_update_amiga_joystick(uint8_t dev_addr) {
    stadia_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) return;

    uint8_t direction = 0;

    if (ctrl->dpad != STADIA_DPAD_NEUTRAL) {
        switch (ctrl->dpad) {
            case STADIA_DPAD_UP:         direction = 0x01; break;
            case STADIA_DPAD_UP_RIGHT:   direction = 0x09; break;
            case STADIA_DPAD_RIGHT:       direction = 0x08; break;
            case STADIA_DPAD_DOWN_RIGHT:  direction = 0x0A; break;
            case STADIA_DPAD_DOWN:        direction = 0x02; break;
            case STADIA_DPAD_DOWN_LEFT:   direction = 0x06; break;
            case STADIA_DPAD_LEFT:        direction = 0x04; break;
            case STADIA_DPAD_UP_LEFT:     direction = 0x05; break;
            default: break;
        }
    } else {
        int16_t dz = ctrl->deadzone;
        if (abs(ctrl->stick_left_x) > dz || abs(ctrl->stick_left_y) > dz) {
            if (ctrl->stick_left_y < -dz) direction |= 0x01;
            if (ctrl->stick_left_y > dz)  direction |= 0x02;
            if (ctrl->stick_left_x < -dz) direction |= 0x04;
            if (ctrl->stick_left_x > dz)  direction |= 0x08;
        }
    }

    bool up    = (direction & 0x01) != 0;
    bool down  = (direction & 0x02) != 0;
    bool left  = (direction & 0x04) != 0;
    bool right = (direction & 0x08) != 0;

    amiga_joystick_port2_set_direction(AJ2_UP, up);
    amiga_joystick_port2_set_direction(AJ2_DOWN, down);
    amiga_joystick_port2_set_direction(AJ2_LEFT, left);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, right);

    bool fire = (ctrl->buttons & (STADIA_BTN_A | STADIA_BTN_B | STADIA_BTN_X | STADIA_BTN_Y | STADIA_BTN_R1 | STADIA_BTN_R2)) != 0
                || ctrl->trigger_right > 128;
    bool button2 = (ctrl->buttons & STADIA_BTN_B) != 0;
    bool button3 = (ctrl->buttons & STADIA_BTN_X) != 0;

    amiga_joystick_port2_set_button(AJ2_FIRE, fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, button2);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, button3);
}

void stadia_mount_cb(uint8_t dev_addr) {
    printf("Stadia: Google Stadia controller detected (addr=%d)\n", dev_addr);

#if HIDPICO_REVISION == 5
    display_show_controller_detected("Stadia", "Google Controller", 2000);
#endif

    stadia_controller_t* ctrl = allocate_controller(dev_addr);
    if (ctrl) {
        printf("Stadia: Controller registered\n");
    } else {
        printf("Stadia: Failed to allocate controller\n");
    }
}

void stadia_unmount_cb(uint8_t dev_addr) {
    printf("Stadia: Controller unmounted (addr=%d)\n", dev_addr);
    free_controller(dev_addr);

    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}

uint8_t stadia_connected_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].connected) count++;
    }
    return count;
}
