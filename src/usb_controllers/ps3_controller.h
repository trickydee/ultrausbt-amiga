/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * PS3 DualShock 3 controller support for Amiga joystick emulation
 * Based on Atari IKBD implementation
 */

#ifndef PS3_CONTROLLER_H
#define PS3_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// PS3 DualShock 3 Identification
//--------------------------------------------------------------------

// Sony Vendor ID
#define PS3_VENDOR_ID       0x054C

// DualShock 3 Product IDs
#define PS3_DS3_PID         0x0268  // DualShock 3 standard

//--------------------------------------------------------------------
// PS3 DualShock 3 Report Structure
//--------------------------------------------------------------------

// PS3 report format
typedef struct TU_ATTR_PACKED {
    // Button states
    uint8_t buttons[3];         // Button states
    
    // Analog sticks (0-255, 128 = center)
    uint8_t lx;                 // Left stick X
    uint8_t ly;                 // Left stick Y
    uint8_t rx;                 // Right stick X
    uint8_t ry;                 // Right stick Y
    
    // D-Pad (0-7 = directions, 8 = center)
    uint8_t dpad;
    
    // Analog triggers (0-255)
    uint8_t l2_trigger;
    uint8_t r2_trigger;
} ps3_report_t;

//--------------------------------------------------------------------
// PS3 Controller State
//--------------------------------------------------------------------

typedef struct {
    uint8_t dev_addr;           // USB device address
    bool connected;             // Connection status
    ps3_report_t report;        // Latest report
    int16_t deadzone;           // Stick deadzone (default 50)
    uint8_t raw_report[64];     // Store raw report for debugging
    uint16_t raw_len;           // Raw report length
} ps3_controller_t;

//--------------------------------------------------------------------
// API Functions
//--------------------------------------------------------------------

/**
 * Check if a device is a PS3 DualShock 3 controller
 * @param vid Vendor ID
 * @param pid Product ID
 * @return true if device is PS3 controller
 */
bool ps3_is_dualshock3(uint16_t vid, uint16_t pid);

/**
 * Process PS3 controller report
 * @param dev_addr USB device address
 * @param report Pointer to HID report data
 * @param len Report length
 * @return true if processed successfully
 */
bool ps3_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);

/**
 * Get PS3 controller state
 * @param dev_addr USB device address
 * @return Pointer to controller state (NULL if not found)
 */
ps3_controller_t* ps3_get_controller(uint8_t dev_addr);

/**
 * Update Amiga joystick port 2 from PS3 controller
 * @param dev_addr USB device address
 */
void ps3_update_amiga_joystick(uint8_t dev_addr);

/**
 * Mount callback
 * @param dev_addr USB device address
 */
void ps3_mount_cb(uint8_t dev_addr);

/**
 * Unmount callback
 * @param dev_addr USB device address
 */
void ps3_unmount_cb(uint8_t dev_addr);

/**
 * Return number of connected PS3 controllers
 */
uint8_t ps3_connected_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PS3_CONTROLLER_H */


