/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * PS3 DualShock 3 controller implementation
 * Based on Atari IKBD implementation
 */

#include "ps3_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include <stdio.h>
#include <string.h>

//--------------------------------------------------------------------
// Controller Storage
//--------------------------------------------------------------------

#define MAX_PS3_CONTROLLERS  2

static ps3_controller_t controllers[MAX_PS3_CONTROLLERS];
static uint8_t controller_count = 0;

//--------------------------------------------------------------------
// Helper Functions
//--------------------------------------------------------------------

static ps3_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected) {
            return &controllers[i];
        }
    }
    return NULL;
}

static ps3_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_PS3_CONTROLLERS) {
        printf("PS3: Max controllers reached\n");
        return NULL;
    }
    
    ps3_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(ps3_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    ctrl->deadzone = 50;  // Match PS4 deadzone
    
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
// Third-party PS3-compatible VID/PIDs (same HID report format as DualShock 3).
// List matches joypad-os sony_ds3.c for arcade sticks and PC/PS3 controllers.

static bool ps3_match_vid_pid(uint16_t vid, uint16_t pid) {
    return (vid == 0x054c && pid == 0x0268)    // Sony DualShock 3
        || (vid == 0x0f0d && (pid == 0x0010 || pid == 0x0011 || pid == 0x0026 || pid == 0x0027 || pid == 0x008b)) // HORI
        || (vid == 0x0738 && (pid == 0x3180 || pid == 0x8818 || pid == 0x8838)) // Mad Catz
        || (vid == 0x2c22 && (pid == 0x2302 || pid == 0x2500))   // Qanba
        || (vid == 0x146b && pid == 0x0904)    // Nacon Daija (PS3)
        || (vid == 0x1292 && pid == 0x4e47)    // Fire NEOGEOX (PS3 HID)
        || (vid == 0x0079 && pid == 0x0006)    // Generic Zero Delay (PC/PS3)
        || (vid == 0x046d && pid == 0xc216);    // Logitech F310 (PS3 Mode)
}

bool ps3_is_dualshock3(uint16_t vid, uint16_t pid) {
    return ps3_match_vid_pid(vid, pid);
}

bool ps3_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    ps3_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        ctrl = allocate_controller(dev_addr);
        if (!ctrl) {
            return false;
        }
    }
    
    // Store raw report
    ctrl->raw_len = len < sizeof(ctrl->raw_report) ? len : sizeof(ctrl->raw_report);
    memcpy(ctrl->raw_report, report, ctrl->raw_len);
    
    // PS3 DualShock 3 report parsing
    // Report format (48 bytes total):
    // Byte 0: Report ID (0x01)
    // Byte 1: Buttons (SELECT=0x01, L3=0x02, R3=0x04, START=0x08, etc)
    // Byte 2: D-Pad (UP=0x10, RIGHT=0x20, DOWN=0x40, LEFT=0x80)
    // Byte 3: Buttons (L2=0x01, R2=0x02, L1=0x04, R1=0x08, Triangle=0x10, Circle=0x20, X=0x40, Square=0x80)
    // Bytes 6-9: Analog sticks (LX, LY, RX, RY) - 0x80 = center
    // Bytes 18-19: Analog triggers (L2, R2) - 0x00 = released, 0xFF = fully pressed
    
    if (len >= 20) {
        uint8_t offset = 0;
        
        // Check if first byte is report ID
        if (report[0] == 0x01) {
            offset = 1;
        }
        
        // Button bytes
        ctrl->report.buttons[0] = report[offset + 0];  // SELECT, L3, R3, START
        ctrl->report.buttons[1] = report[offset + 1];  // D-Pad
        ctrl->report.buttons[2] = report[offset + 2];  // L2, R2, L1, R1, Triangle, Circle, X, Square
        
        // Analog sticks (bytes 6-9 in report, or 5-8 if offset=1)
        // 0x00 = full left/up, 0x80 = center, 0xFF = full right/down
        if (len >= offset + 9) {
            ctrl->report.lx = report[offset + 5];  // Left stick X
            ctrl->report.ly = report[offset + 6];  // Left stick Y  
            ctrl->report.rx = report[offset + 7];  // Right stick X
            ctrl->report.ry = report[offset + 8];  // Right stick Y
        }
        
        // Analog triggers (bytes 18-19, or 17-18 if offset=1)
        if (len >= offset + 19) {
            ctrl->report.l2_trigger = report[offset + 17];
            ctrl->report.r2_trigger = report[offset + 18];
        }
        
        // Extract D-Pad from byte 1 (bitmask format)
        // UP=0x10, RIGHT=0x20, DOWN=0x40, LEFT=0x80
        uint8_t dpad_byte = ctrl->report.buttons[1];
        ctrl->report.dpad = 8;  // Default = centered
        
        // Convert bitmask to hat switch format (0-7, 8=center)
        if (dpad_byte & 0x10) {  // UP
            if (dpad_byte & 0x20) ctrl->report.dpad = 1;      // UP-RIGHT
            else if (dpad_byte & 0x80) ctrl->report.dpad = 7; // UP-LEFT  
            else ctrl->report.dpad = 0;                       // UP
        } else if (dpad_byte & 0x40) {  // DOWN
            if (dpad_byte & 0x20) ctrl->report.dpad = 3;      // DOWN-RIGHT
            else if (dpad_byte & 0x80) ctrl->report.dpad = 5; // DOWN-LEFT
            else ctrl->report.dpad = 4;                       // DOWN
        } else if (dpad_byte & 0x20) {  // RIGHT
            ctrl->report.dpad = 2;
        } else if (dpad_byte & 0x80) {  // LEFT
            ctrl->report.dpad = 6;
        }
    }
    
    // Update Amiga joystick port 2
    ps3_update_amiga_joystick(dev_addr);
    
    return true;
}

ps3_controller_t* ps3_get_controller(uint8_t dev_addr) {
    return find_controller_by_addr(dev_addr);
}

void ps3_update_amiga_joystick(uint8_t dev_addr) {
    ps3_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) {
        return;
    }
    
    const ps3_report_t* input = &ctrl->report;
    
    // Compute direction from left stick or D-pad
    uint8_t direction = 0;
    int8_t stick_x = (int8_t)(input->lx - 128);
    int8_t stick_y = (int8_t)(input->ly - 128);
    
    // Check if D-pad is active (takes priority)
    if (input->dpad != 8) {
        // D-pad active - use it
        switch (input->dpad) {
            case 0: direction = 0x01; break;  // UP
            case 1: direction = 0x09; break;  // UP-RIGHT
            case 2: direction = 0x08; break;  // RIGHT
            case 3: direction = 0x0A; break;  // DOWN-RIGHT
            case 4: direction = 0x02; break;  // DOWN
            case 5: direction = 0x06; break;  // DOWN-LEFT
            case 6: direction = 0x04; break;  // LEFT
            case 7: direction = 0x05; break;  // UP-LEFT
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
    amiga_joystick_port2_set_direction(AJ2_UP, (direction & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN, (direction & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT, (direction & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, (direction & 0x08) != 0);
    
    // Map buttons
    // X button (0x40) = Fire
    // Circle button (0x20) = Button 2
    // Square button (0x80) = Button 3
    amiga_joystick_port2_set_button(AJ2_FIRE, (input->buttons[2] & 0x40) != 0);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, (input->buttons[2] & 0x20) != 0);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, (input->buttons[2] & 0x80) != 0);
}

void ps3_mount_cb(uint8_t dev_addr) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  🎮 PS3 DUALSHOCK 3 DETECTED!\n");
    printf("  Device Address: %d\n", dev_addr);
    printf("  \n");
    printf("  Sending PS3 initialization command...\n");
    printf("  \n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("\n");
    
#if HIDPICO_REVISION == 5
    // Show on OLED - match Atari IKBD style
    display_show_controller_detected("PS3", "DualShock 3", 3000);
#endif
    
    ps3_controller_t* ctrl = allocate_controller(dev_addr);
    if (ctrl) {
        printf("PS3: Controller registered!\n");
        
        // PS3 DualShock 3 requires special initialization
        // Send Feature Report 0xF4 to enable the controller
        static const uint8_t ps3_init_report[] = {
            0x42, 0x0C, 0x00, 0x00  // PS3 enable command
        };
        
        printf("PS3: Sending initialization feature report (0xF4)...\n");
        
        // Send feature report to initialize controller
        bool result = tuh_hid_set_report(dev_addr, 0, // instance 0
                                          0xF4,        // report_id
                                          HID_REPORT_TYPE_FEATURE,
                                          (uint8_t*)ps3_init_report, 
                                          sizeof(ps3_init_report));
        
        if (result) {
            printf("PS3: Initialization sent successfully!\n");
        } else {
            printf("PS3: WARNING - Initialization send failed!\n");
        }
    }
}

void ps3_unmount_cb(uint8_t dev_addr) {
    printf("PS3: Controller unmounted at address %d\n", dev_addr);
    free_controller(dev_addr);
    
    // Reset joystick port 2
    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}

uint8_t ps3_connected_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].connected) {
            count++;
        }
    }
    return count;
}

