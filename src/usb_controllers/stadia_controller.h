/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Google Stadia controller support for Amiga joystick emulation
 * Based on Atari IKBD implementation
 */

#ifndef STADIA_CONTROLLER_H
#define STADIA_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// Google Stadia Controller Identification
//--------------------------------------------------------------------

#define STADIA_VENDOR_ID        0x18D1
#define STADIA_CONTROLLER_PID   0x9400  // Stadia Controller rev. A

//--------------------------------------------------------------------
// Stadia Controller State
//--------------------------------------------------------------------

typedef struct {
    uint8_t dev_addr;
    bool connected;

    uint16_t buttons;
    int16_t stick_left_x;
    int16_t stick_left_y;
    int16_t stick_right_x;
    int16_t stick_right_y;
    uint8_t trigger_left;
    uint8_t trigger_right;
    uint8_t dpad;

    int16_t deadzone;
} stadia_controller_t;

//--------------------------------------------------------------------
// Stadia Button Definitions
//--------------------------------------------------------------------

#define STADIA_BTN_A            0x0001
#define STADIA_BTN_B            0x0002
#define STADIA_BTN_X            0x0004
#define STADIA_BTN_Y            0x0008
#define STADIA_BTN_L1           0x0010
#define STADIA_BTN_R1           0x0020
#define STADIA_BTN_L2           0x0040
#define STADIA_BTN_R2           0x0080
#define STADIA_BTN_SELECT       0x0100
#define STADIA_BTN_START        0x0200
#define STADIA_BTN_L3           0x0400
#define STADIA_BTN_R3           0x0800
#define STADIA_BTN_HOME         0x1000
#define STADIA_BTN_CAPTURE      0x2000

// D-Pad (hat) values
#define STADIA_DPAD_UP          0
#define STADIA_DPAD_UP_RIGHT     1
#define STADIA_DPAD_RIGHT        2
#define STADIA_DPAD_DOWN_RIGHT   3
#define STADIA_DPAD_DOWN         4
#define STADIA_DPAD_DOWN_LEFT    5
#define STADIA_DPAD_LEFT         6
#define STADIA_DPAD_UP_LEFT      7
#define STADIA_DPAD_NEUTRAL      15

//--------------------------------------------------------------------
// API
//--------------------------------------------------------------------

bool stadia_is_controller(uint16_t vid, uint16_t pid);
void stadia_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);
stadia_controller_t* stadia_get_controller(uint8_t dev_addr);
void stadia_update_amiga_joystick(uint8_t dev_addr);
void stadia_mount_cb(uint8_t dev_addr);
void stadia_unmount_cb(uint8_t dev_addr);
uint8_t stadia_connected_count(void);

#ifdef __cplusplus
}
#endif

#endif /* STADIA_CONTROLLER_H */
