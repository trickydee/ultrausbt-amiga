/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Sony PlayStation Classic controller support for Amiga joystick emulation.
 * Report format matches joypad-os sony_psc (3-byte HID report, no analog sticks).
 */

#ifndef PSC_CONTROLLER_H
#define PSC_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PSC_VENDOR_ID   0x054C
#define PSC_PID         0x0CDA  // Sony PlayStation Classic

bool psc_is_controller(uint16_t vid, uint16_t pid);
void psc_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);
void psc_update_amiga_joystick(uint8_t dev_addr);
void psc_mount_cb(uint8_t dev_addr);
void psc_unmount_cb(uint8_t dev_addr);
uint8_t psc_connected_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PSC_CONTROLLER_H */
