/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Xbox (XInput) controller support for Amiga joystick emulation.
 * Based on Atari IKBD XInput implementation (xinput_host + xinput_atari).
 */

#ifndef XBOX_CONTROLLER_H
#define XBOX_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if CFG_TUH_XINPUT

/**
 * Return number of connected Xbox controllers.
 */
uint8_t xbox_connected_count(void);

#endif /* CFG_TUH_XINPUT */

/**
 * HID path: detect Xbox controller by VID/PID (for pads that expose HID only).
 */
bool xbox_is_hid_controller(uint16_t vid, uint16_t pid);

/**
 * HID mount/unmount for Xbox controller (display + joystick clear on unmount).
 */
void xbox_hid_mount_cb(uint8_t dev_addr);
void xbox_hid_umount_cb(uint8_t dev_addr);

#ifdef __cplusplus
}
#endif

#endif /* XBOX_CONTROLLER_H */
