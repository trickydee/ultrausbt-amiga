/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * PS5 DualSense controller support for Amiga joystick emulation.
 * Report format from Bluepad32 uni_hid_parser_ds5 (USB: report ID 0x31, 78 bytes).
 */

#ifndef PS5_CONTROLLER_H
#define PS5_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// PS5 DualSense Identification
//--------------------------------------------------------------------

// Sony Vendor ID (same as PS3/PS4)
#define PS5_VENDOR_ID       0x054C

// DualSense Product IDs
#define PS5_DUALENSE_PID    0x0CE6  // DualSense (PS5)
#define PS5_DUALENSE_EDGE_PID 0x0DF2  // DualSense Edge

//--------------------------------------------------------------------
// PS5 DualSense Input Report (minimal, for joystick mapping)
// USB report: first byte 0x31, then payload at offset 2.
// We only parse sticks and first two button bytes.
//--------------------------------------------------------------------

#define PS5_REPORT_ID       0x31
#define PS5_REPORT_LEN      78

typedef struct TU_ATTR_PACKED {
    uint8_t x, y;           // Left stick (0-255, 127 = center)
    uint8_t rx, ry;         // Right stick
    uint8_t brake, throttle;// L2, R2 analog (0-255)
    uint8_t seq_number;
    uint8_t buttons[4];     // buttons[0]: d-pad lo nibble, Square/X/Circle/Triangle; buttons[1]: L1 R1 L2 R2 Share Options L3 R3
} ps5_report_mini_t;

// D-pad values (buttons[0] & 0x0F)
#define PS5_DPAD_UP         0
#define PS5_DPAD_UP_RIGHT   1
#define PS5_DPAD_RIGHT       2
#define PS5_DPAD_DOWN_RIGHT  3
#define PS5_DPAD_DOWN        4
#define PS5_DPAD_DOWN_LEFT   5
#define PS5_DPAD_LEFT        6
#define PS5_DPAD_UP_LEFT     7
#define PS5_DPAD_CENTER      8   // or 0x0F when released

//--------------------------------------------------------------------
// PS5 Controller State
//--------------------------------------------------------------------

typedef struct {
    uint8_t dev_addr;
    bool connected;
    ps5_report_mini_t report;
    int16_t deadzone;
} ps5_controller_t;

//--------------------------------------------------------------------
// API Functions
//--------------------------------------------------------------------

/**
 * Check if a device is a PS5 DualSense controller
 */
bool ps5_is_dualsense(uint16_t vid, uint16_t pid);

/**
 * Process DualSense input report (78 bytes, report ID 0x31).
 */
bool ps5_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);

/**
 * Get controller state (NULL if not found).
 */
ps5_controller_t* ps5_get_controller(uint8_t dev_addr);

/**
 * Update Amiga joystick port 2 from DualSense state.
 */
void ps5_update_amiga_joystick(uint8_t dev_addr);

/**
 * Mount callback (call when HID device is mounted).
 */
void ps5_mount_cb(uint8_t dev_addr);

/**
 * Unmount callback (call when HID device is unmounted).
 */
void ps5_unmount_cb(uint8_t dev_addr);

/**
 * Number of connected DualSense controllers.
 */
uint8_t ps5_connected_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PS5_CONTROLLER_H */
