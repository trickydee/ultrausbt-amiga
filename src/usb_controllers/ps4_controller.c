/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * PS4 DualShock 4 controller implementation
 * Based on Atari IKBD implementation
 */

#include "ps4_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "platform/amiga/port2_gamepad.h"
#include "display/display.h"
#include "usb_device_map.h"
#include <stdio.h>
#include <string.h>

//--------------------------------------------------------------------
// Controller Storage
//--------------------------------------------------------------------

#define MAX_PS4_CONTROLLERS  2

static ps4_controller_t controllers[MAX_PS4_CONTROLLERS];
static uint8_t controller_count = 0;

//--------------------------------------------------------------------
// Helper Functions
//--------------------------------------------------------------------

static ps4_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected) {
            return &controllers[i];
        }
    }
    return NULL;
}

static ps4_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_PS4_CONTROLLERS) {
        printf("PS4: Max controllers reached\n");
        return NULL;
    }
    
    ps4_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(ps4_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    ctrl->deadzone = 20;  // Reduced deadzone for better sensitivity (was 50)
    
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
// Third-party PS4-compatible VID/PIDs (same HID report format as DualShock 4).
// List matches joypad-os sony_ds4.c for arcade sticks and PS4 controllers.

static bool ps4_match_vid_pid(uint16_t vid, uint16_t pid) {
    return (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4 || pid == 0x0ba0)) // Sony DS4 + dongle
        || (vid == 0x0f0d && (pid == 0x005e || pid == 0x0066 || pid == 0x008a || pid == 0x00ee)) // HORI
        || (vid == 0x1532 && (pid == 0x0401 || pid == 0x1004 || pid == 0x1008)) // Razer
        || (vid == 0x0c12 && (pid == 0x0c30 || pid == 0x0ef7 || pid == 0x1e1b)) // Brook, Feir
        || (vid == 0x0738 && (pid == 0x8180 || pid == 0x8384 || pid == 0x8481)) // Mad Catz
        || (vid == 0x2c22 && (pid == 0x2000 || pid == 0x2200 || pid == 0x2300))  // Qanba
        || (vid == 0x146b && pid == 0x0d09)    // Nacon Daija (PS4)
        || (vid == 0x20d6 && pid == 0x792a)    // PowerA FUSION FightPad
        || (vid == 0x1f4f && pid == 0x1002)    // ASW Guilty Gear xrd
        || (vid == 0x04d8 && pid == 0x1529);   // Universal PCB Project (UPCB)
}

bool ps4_is_dualshock4(uint16_t vid, uint16_t pid) {
    return ps4_match_vid_pid(vid, pid);
}

bool ps4_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    static bool first_report = true;
    
    ps4_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) {
            return false;
        }
    }
    
    // PS4 reports are at least 9 bytes
    if (len < 9) {
        printf("PS4: Report too short (%d bytes)\n", len);
        return false;
    }
    
    if (first_report) {
        first_report = false;
        printf("PS4: First report received (len=%d)\n", len);
        printf("PS4: First 16 bytes: ");
        for (int i = 0; i < (len < 16 ? len : 16); i++) {
            printf("%02X ", report[i]);
        }
        printf("\n");
    }
    
    // Parse PS4 report
    // Note: USB PS4 reports typically start at byte 0, but may have report ID
    ps4_report_t* input = &ctrl->report;
    
    uint8_t offset = 0;
    
    // If first byte looks like report ID (0x01, 0x11, etc), skip it
    if (report[0] == 0x01 || report[0] == 0x11) {
        offset = 1;  // Skip report ID
    }
    
    input->x = report[offset + 0];
    input->y = report[offset + 1];
    input->z = report[offset + 2];        // Right stick X
    input->rz = report[offset + 3];       // Right stick Y
    
    // Buttons in byte 4
    uint8_t buttons1 = report[offset + 4];
    input->dpad = buttons1 & 0x0F;
    input->square = (buttons1 >> 4) & 1;
    input->cross = (buttons1 >> 5) & 1;
    input->circle = (buttons1 >> 6) & 1;
    input->triangle = (buttons1 >> 7) & 1;
    
    // Buttons in byte 5
    uint8_t buttons2 = report[offset + 5];
    input->l1 = buttons2 & 1;
    input->r1 = (buttons2 >> 1) & 1;
    input->l2 = (buttons2 >> 2) & 1;
    input->r2 = (buttons2 >> 3) & 1;
    input->share = (buttons2 >> 4) & 1;
    input->options = (buttons2 >> 5) & 1;
    input->l3 = (buttons2 >> 6) & 1;
    input->r3 = (buttons2 >> 7) & 1;
    
    // PS button and touchpad in byte 6
    if (len > offset + 6) {
        uint8_t buttons3 = report[offset + 6];
        input->ps = buttons3 & 1;
        input->tpad = (buttons3 >> 1) & 1;
        input->counter = (buttons3 >> 2) & 0x3F;
    }
    
    // Analog triggers in bytes 7-8
    if (len > offset + 8) {
        input->l2_trigger = report[offset + 7];
        input->r2_trigger = report[offset + 8];
    }
    
    // Debug: Print parsed values every 100 reports
    static uint32_t report_count = 0;
    report_count++;
    if ((report_count % 100) == 0) {
        printf("PS4: Report #%lu - x=%d y=%d dpad=%d cross=%d circle=%d square=%d\n",
               report_count, input->x, input->y, input->dpad, input->cross, input->circle, input->square);
    }
    
    // Update Amiga joystick port 2
    ps4_update_amiga_joystick(dev_addr);
    
    return true;
}

ps4_controller_t* ps4_get_controller(uint8_t dev_addr) {
    return find_controller_by_addr(dev_addr);
}

void ps4_update_amiga_joystick(uint8_t dev_addr) {
    ps4_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        return;
    }
    
    const ps4_report_t* input = &ctrl->report;
    
    // Compute direction from left stick or D-pad
    uint8_t direction = 0;
    // Convert stick values from 0-255 (128=center) to -128 to +127
    int16_t stick_x = (int16_t)input->x - 128;
    int16_t stick_y = (int16_t)input->y - 128;
    
    // Check if D-pad is active (takes priority)
    if (input->dpad != PS4_DPAD_CENTER) {
        // D-pad active - use it
        switch (input->dpad) {
            case PS4_DPAD_UP:         direction = 0x01; break;
            case PS4_DPAD_UP_RIGHT:   direction = 0x09; break;
            case PS4_DPAD_RIGHT:      direction = 0x08; break;
            case PS4_DPAD_DOWN_RIGHT: direction = 0x0A; break;
            case PS4_DPAD_DOWN:       direction = 0x02; break;
            case PS4_DPAD_DOWN_LEFT:  direction = 0x06; break;
            case PS4_DPAD_LEFT:       direction = 0x04; break;
            case PS4_DPAD_UP_LEFT:    direction = 0x05; break;
        }
    } else {
        // Use analog stick
        if (stick_x < -ctrl->deadzone || stick_x > ctrl->deadzone ||
            stick_y < -ctrl->deadzone || stick_y > ctrl->deadzone) {
            
            if (stick_y < -ctrl->deadzone) direction |= 0x01;  // UP
            if (stick_y > ctrl->deadzone)  direction |= 0x02;    // DOWN
            if (stick_x < -ctrl->deadzone) direction |= 0x04;  // LEFT
            if (stick_x > ctrl->deadzone)  direction |= 0x08;  // RIGHT
        }
    }
    
    // Map direction bits to Amiga joystick port 2
    bool up = (direction & 0x01) != 0;
    bool down = (direction & 0x02) != 0;
    bool left = (direction & 0x04) != 0;
    bool right = (direction & 0x08) != 0;
    uint8_t dir_bits = (up ? 0x01 : 0) | (down ? 0x02 : 0) | (left ? 0x04 : 0) | (right ? 0x08 : 0);

    bool fire = input->cross || (input->r2_trigger > 128);
    bool button2 = input->circle;
    bool button3 = input->square;
    bool l_trig = input->l2_trigger > 128;
    bool r_trig = input->r2_trigger > 128;
    bool start = input->options;

    (void)fire;
    (void)button2;
    (void)button3;

    port2_gamepad_submit(dir_bits,
                         input->cross || (input->r2_trigger > 128),
                         input->circle,
                         input->square,
                         input->triangle,
                         l_trig,
                         r_trig,
                         start);
}

void ps4_mount_cb(uint8_t dev_addr) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  🎮 PS4 DUALSHOCK 4 DETECTED!\n");
    printf("  Device Address: %d\n", dev_addr);
    printf("  \n");
    printf("  PS4 controllers are standard HID devices\n");
    printf("  Should work immediately with TinyUSB 0.19.0!\n");
    printf("  \n");
    printf("  Button mapping:\n");
    printf("  - Left Stick / D-Pad = Directions\n");
    printf("  - Cross (X) = Fire\n");
    printf("  - Circle = Button 2\n");
    printf("  - Square = Button 3\n");
    printf("  - R2 Trigger = Fire (alternative)\n");
    printf("  \n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("\n");
    
#if HIDPICO_REVISION == 5
    // Show on OLED - match Atari IKBD style
    display_show_controller_detected("PS4", "DualShock 4", 3000);
#endif
    
    ps4_controller_t* ctrl = allocate_controller(dev_addr);
    if (ctrl) {
        printf("PS4: Controller registered!\n");
        usb_map_register_gamepad(dev_addr, "PS4");
    }
}

void ps4_unmount_cb(uint8_t dev_addr) {
    printf("PS4: Controller unmounted at address %d\n", dev_addr);
    usb_map_unregister_gamepad(dev_addr);
    free_controller(dev_addr);
    
    // Reset joystick port 2
    port2_gamepad_clear();
}

uint8_t ps4_connected_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].connected) {
            count++;
        }
    }
    return count;
}

