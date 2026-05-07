/**
 * Nintendo Switch Pro Controller (and compatible) support for Amiga joystick emulation
 * Based on Atari IKBD implementation
 */

#ifndef SWITCH_CONTROLLER_H
#define SWITCH_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWITCH_VENDOR_ID        0x057E
#define SWITCH_PRO_CONTROLLER   0x2009
#define SWITCH_JOYCON_L         0x2006
#define SWITCH_JOYCON_R         0x2007
#define SWITCH_JOYCON_PAIR      0x2008
#define SWITCH_JOYCON_GRIP      0x200E  // JoyCon Charge Grip (same report as Pro)
#define SWITCH_SNES_NSO         0x2017  // SNES Controller (NSO)

#define POWERA_VENDOR_ID        0x20D6
#define POWERA_FUSION_ARCADE    0xA711
#define POWERA_FUSION_ARCADE_V2 0xA715
#define POWERA_WIRED_PLUS       0xA712
#define POWERA_WIRELESS        0xA713

typedef struct {
    uint8_t dev_addr;
    bool connected;
    uint16_t buttons;
    int16_t stick_left_x;
    int16_t stick_left_y;
    int16_t stick_right_x;
    int16_t stick_right_y;
    uint8_t dpad;
    int16_t deadzone;
} switch_controller_t;

#define SWITCH_BTN_Y            0x0001
#define SWITCH_BTN_B            0x0002
#define SWITCH_BTN_A            0x0004
#define SWITCH_BTN_X            0x0008
#define SWITCH_BTN_L            0x0010
#define SWITCH_BTN_R            0x0020
#define SWITCH_BTN_ZL           0x0040
#define SWITCH_BTN_ZR           0x0080
#define SWITCH_BTN_MINUS        0x0100
#define SWITCH_BTN_PLUS         0x0200
#define SWITCH_BTN_LSTICK       0x0400
#define SWITCH_BTN_RSTICK       0x0800
#define SWITCH_BTN_HOME         0x1000
#define SWITCH_BTN_CAPTURE      0x2000

#define SWITCH_DPAD_UP          0
#define SWITCH_DPAD_UP_RIGHT    1
#define SWITCH_DPAD_RIGHT       2
#define SWITCH_DPAD_DOWN_RIGHT  3
#define SWITCH_DPAD_DOWN        4
#define SWITCH_DPAD_DOWN_LEFT   5
#define SWITCH_DPAD_LEFT        6
#define SWITCH_DPAD_UP_LEFT     7
#define SWITCH_DPAD_NEUTRAL     15

bool switch_is_controller(uint16_t vid, uint16_t pid);
void switch_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len);
switch_controller_t* switch_get_controller(uint8_t dev_addr);
void switch_update_amiga_joystick(uint8_t dev_addr);
void switch_mount_cb(uint8_t dev_addr);
void switch_unmount_cb(uint8_t dev_addr);
uint8_t switch_connected_count(void);

/** Call periodically from main loop to run delayed Pro Controller init (1 s after mount). */
void switch_check_delayed_init(void);

#ifdef __cplusplus
}
#endif

#endif /* SWITCH_CONTROLLER_H */
