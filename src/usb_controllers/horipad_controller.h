/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * HORI HORIPAD for Nintendo Switch (0x0F0D / 0x00C1).
 * Report format matches joypad-os hori_horipad_report_t (buttons, dpad, 4 axes).
 */

#ifndef HORIPAD_CONTROLLER_H
#define HORIPAD_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HORIPAD_VENDOR_ID  0x0F0D
#define HORIPAD_PID        0x00C1  // HORI HORIPAD (Switch)

bool horipad_is_controller(uint16_t vid, uint16_t pid);
void horipad_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);
void horipad_update_amiga_joystick(uint8_t dev_addr);
void horipad_mount_cb(uint8_t dev_addr);
void horipad_unmount_cb(uint8_t dev_addr);
uint8_t horipad_connected_count(void);

#ifdef __cplusplus
}
#endif

#endif /* HORIPAD_CONTROLLER_H */
