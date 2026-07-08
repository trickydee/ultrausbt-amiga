/**
 * Nintendo Switch Pro Controller (and compatible) implementation
 * Based on Atari IKBD implementation
 */

#include "switch_controller.h"
#include "config.h"
#include "tusb.h"
#include "platform/amiga/joystick_port2.h"
#include "display/display.h"
#include "usb_device_map.h"
#include "pico/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SWITCH_CONTROLLERS  2
#define PRO_INIT_DELAY_MS       1000

static switch_controller_t controllers[MAX_SWITCH_CONTROLLERS];
static uint8_t controller_count = 0;
static uint8_t global_count = 0;

static bool pro_needs_init = false;
static uint8_t pro_dev_addr = 0;
static uint32_t pro_mount_time = 0;
static bool pro_init_attempted = false;

static switch_controller_t* find_controller_by_addr(uint8_t dev_addr) {
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].dev_addr == dev_addr && controllers[i].connected) {
            return &controllers[i];
        }
    }
    return NULL;
}

static switch_controller_t* allocate_controller(uint8_t dev_addr) {
    if (controller_count >= MAX_SWITCH_CONTROLLERS) {
        printf("Switch: Max controllers reached\n");
        return NULL;
    }
    switch_controller_t* ctrl = &controllers[controller_count++];
    memset(ctrl, 0, sizeof(switch_controller_t));
    ctrl->dev_addr = dev_addr;
    ctrl->connected = true;
    ctrl->deadzone = 20;
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

bool switch_is_controller(uint16_t vid, uint16_t pid) {
    if (vid == SWITCH_VENDOR_ID) {
        switch (pid) {
            case SWITCH_PRO_CONTROLLER:
            case SWITCH_JOYCON_L:
            case SWITCH_JOYCON_R:
            case SWITCH_JOYCON_PAIR:
            case SWITCH_JOYCON_GRIP:   // JoyCon Charge Grip (same report as Pro)
            case SWITCH_SNES_NSO:     // SNES Controller (NSO)
                return true;
        }
    }
    if (vid == POWERA_VENDOR_ID) {
        switch (pid) {
            case POWERA_FUSION_ARCADE:
            case POWERA_FUSION_ARCADE_V2:
            case POWERA_WIRED_PLUS:
            case POWERA_WIRELESS:
                return true;
        }
    }
    return false;
}

static bool send_usb_command(uint8_t dev_addr, uint8_t cmd) {
    uint8_t buf[2] = {0x80, cmd};
    for (int i = 0; i < 10; i++) {
        tuh_task();
        sleep_ms(1);
    }
    bool result = tuh_hid_send_report(dev_addr, 0, 0, buf, 2);
    for (int i = 0; i < 150; i++) {
        tuh_task();
        sleep_ms(1);
    }
    return result;
}

static bool send_subcommand(uint8_t dev_addr, uint8_t subcmd, const uint8_t* data, uint8_t data_len) {
    uint8_t buf[64] = {0};
    buf[0] = 0x01;
    buf[1] = global_count;
    buf[2] = 0x00; buf[3] = 0x01; buf[4] = 0x40; buf[5] = 0x40;
    buf[6] = 0x00; buf[7] = 0x01; buf[8] = 0x40; buf[9] = 0x40;
    buf[10] = subcmd;
    if (data && data_len > 0) {
        memcpy(&buf[11], data, data_len);
    }
    global_count = (global_count + 1) & 0x0F;
    for (int i = 0; i < 10; i++) {
        tuh_task();
        sleep_ms(1);
    }
    bool result = tuh_hid_send_report(dev_addr, 0, 0, buf, 11 + data_len);
    for (int i = 0; i < 200; i++) {
        tuh_task();
        sleep_ms(1);
    }
    return result;
}

static bool switch_init_pro_controller(uint8_t dev_addr) {
    global_count = 0;
    if (!send_usb_command(dev_addr, 0x02)) return false;
    if (!send_usb_command(dev_addr, 0x03)) return false;
    if (!send_usb_command(dev_addr, 0x02)) return false;
    if (!send_usb_command(dev_addr, 0x04)) return false;
    sleep_ms(100);
    uint8_t imu = 0x01;
    if (!send_subcommand(dev_addr, 0x40, &imu, 1)) return false;
    uint8_t vib = 0x01;
    if (!send_subcommand(dev_addr, 0x48, &vib, 1)) return false;
    uint8_t mode = 0x30;
    if (!send_subcommand(dev_addr, 0x03, &mode, 1)) return false;
    printf("Switch Pro Controller initialized\n");
    return true;
}

void switch_check_delayed_init(void) {
    if (!pro_needs_init || pro_init_attempted) return;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if ((current_time - pro_mount_time) < PRO_INIT_DELAY_MS) return;
    pro_init_attempted = true;
    pro_needs_init = false;
    switch_init_pro_controller(pro_dev_addr);
}

void switch_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
    if (!report || len == 0) return;

    switch_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) return;

    if (len >= 49) {
        uint8_t right_btns = report[3];
        uint8_t mid_btns = report[4];
        uint8_t left_btns = report[5];

        ctrl->buttons = 0;
        if (right_btns & 0x01) ctrl->buttons |= SWITCH_BTN_Y;
        if (right_btns & 0x02) ctrl->buttons |= SWITCH_BTN_X;
        if (right_btns & 0x04) ctrl->buttons |= SWITCH_BTN_B;
        if (right_btns & 0x08) ctrl->buttons |= SWITCH_BTN_A;
        if (right_btns & 0x40) ctrl->buttons |= SWITCH_BTN_R;
        if (right_btns & 0x80) ctrl->buttons |= SWITCH_BTN_ZR;
        if (left_btns & 0x40) ctrl->buttons |= SWITCH_BTN_L;
        if (left_btns & 0x80) ctrl->buttons |= SWITCH_BTN_ZL;
        if (mid_btns & 0x01) ctrl->buttons |= SWITCH_BTN_MINUS;
        if (mid_btns & 0x02) ctrl->buttons |= SWITCH_BTN_PLUS;
        if (mid_btns & 0x10) ctrl->buttons |= SWITCH_BTN_HOME;

        ctrl->dpad = 8;
        if ((left_btns & 0x01) && (left_btns & 0x04)) ctrl->dpad = SWITCH_DPAD_DOWN_RIGHT;
        else if ((left_btns & 0x01) && (left_btns & 0x08)) ctrl->dpad = SWITCH_DPAD_DOWN_LEFT;
        else if ((left_btns & 0x02) && (left_btns & 0x04)) ctrl->dpad = SWITCH_DPAD_UP_RIGHT;
        else if ((left_btns & 0x02) && (left_btns & 0x08)) ctrl->dpad = SWITCH_DPAD_UP_LEFT;
        else if (left_btns & 0x02) ctrl->dpad = SWITCH_DPAD_UP;
        else if (left_btns & 0x01) ctrl->dpad = SWITCH_DPAD_DOWN;
        else if (left_btns & 0x08) ctrl->dpad = SWITCH_DPAD_LEFT;
        else if (left_btns & 0x04) ctrl->dpad = SWITCH_DPAD_RIGHT;

        uint16_t lx_12 = report[6] | ((report[7] & 0x0F) << 8);
        uint16_t ly_12 = (report[7] >> 4) | (report[8] << 4);
        uint16_t rx_12 = report[9] | ((report[10] & 0x0F) << 8);
        uint16_t ry_12 = (report[10] >> 4) | (report[11] << 4);

        #define DZ12 256
        int32_t lx_d = (int32_t)lx_12 - 2048;
        int32_t ly_d = 2048 - (int32_t)ly_12;
        int32_t rx_d = (int32_t)rx_12 - 2048;
        int32_t ry_d = 2048 - (int32_t)ry_12;
        if (lx_d > -DZ12 && lx_d < DZ12) lx_d = 0;
        if (ly_d > -DZ12 && ly_d < DZ12) ly_d = 0;
        if (rx_d > -DZ12 && rx_d < DZ12) rx_d = 0;
        if (ry_d > -DZ12 && ry_d < DZ12) ry_d = 0;

        ctrl->stick_left_x = lx_d / 16;
        ctrl->stick_left_y = ly_d / 16;
        ctrl->stick_right_x = rx_d / 16;
        ctrl->stick_right_y = ry_d / 16;
    } else if (len >= 7) {
        ctrl->buttons = report[0] | (report[1] << 8);
        ctrl->dpad = report[2];
        if (ctrl->dpad >= 8) ctrl->dpad = SWITCH_DPAD_NEUTRAL;
        ctrl->stick_left_x = (int16_t)report[3] - 128;
        ctrl->stick_left_y = 128 - (int16_t)report[4];
        ctrl->stick_right_x = (int16_t)report[5] - 128;
        ctrl->stick_right_y = 128 - (int16_t)report[6];
    }

    switch_update_amiga_joystick(dev_addr);
}

switch_controller_t* switch_get_controller(uint8_t dev_addr) {
    return find_controller_by_addr(dev_addr);
}

void switch_update_amiga_joystick(uint8_t dev_addr) {
    switch_controller_t* ctrl = find_controller_by_addr(dev_addr);
    if (!ctrl) return;

    uint8_t direction = 0;
    if (ctrl->dpad < 8) {
        switch (ctrl->dpad) {
            case SWITCH_DPAD_UP:         direction = 0x01; break;
            case SWITCH_DPAD_UP_RIGHT:   direction = 0x09; break;
            case SWITCH_DPAD_RIGHT:      direction = 0x08; break;
            case SWITCH_DPAD_DOWN_RIGHT: direction = 0x0A; break;
            case SWITCH_DPAD_DOWN:       direction = 0x02; break;
            case SWITCH_DPAD_DOWN_LEFT:  direction = 0x06; break;
            case SWITCH_DPAD_LEFT:       direction = 0x04; break;
            case SWITCH_DPAD_UP_LEFT:    direction = 0x05; break;
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

    bool fire = (ctrl->buttons & (SWITCH_BTN_A | SWITCH_BTN_B | SWITCH_BTN_ZR)) != 0;
    bool button2 = (ctrl->buttons & SWITCH_BTN_B) != 0;
    bool button3 = (ctrl->buttons & SWITCH_BTN_X) != 0;
    amiga_joystick_port2_set_button(AJ2_FIRE, fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, button2);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, button3);
}

void switch_mount_cb(uint8_t dev_addr) {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    const char* controller_name = "Switch";
    if (vid == SWITCH_VENDOR_ID) {
        if (pid == SWITCH_PRO_CONTROLLER) controller_name = "Pro Controller";
        else if (pid == SWITCH_JOYCON_L) controller_name = "Joy-Con Left";
        else if (pid == SWITCH_JOYCON_R) controller_name = "Joy-Con Right";
        else if (pid == SWITCH_JOYCON_PAIR) controller_name = "Joy-Con Pair";
        else if (pid == SWITCH_JOYCON_GRIP) controller_name = "Joy-Con Grip";
        else if (pid == SWITCH_SNES_NSO) controller_name = "SNES NSO";
    } else if (vid == POWERA_VENDOR_ID) {
        if (pid == POWERA_FUSION_ARCADE || pid == POWERA_FUSION_ARCADE_V2) controller_name = "PowerA Arcade";
        else controller_name = "PowerA Controller";
    }

    const char* model = "Controller";
    if (vid == SWITCH_VENDOR_ID && pid == SWITCH_PRO_CONTROLLER) {
        model = "Pro Controller";
    } else if (vid == POWERA_VENDOR_ID) {
        model = "PowerA";
    }

    printf("Switch controller mount: %s (addr=%d)\n", controller_name, dev_addr);

#if HIDPICO_REV_ATARI_BOARD
    display_show_controller_detected("Switch", model, 3000);
#endif

    switch_controller_t* ctrl = allocate_controller(dev_addr);
    if (ctrl) {
        usb_map_register_gamepad(dev_addr, controller_name);
        if (vid == SWITCH_VENDOR_ID && pid == SWITCH_PRO_CONTROLLER) {
            pro_needs_init = true;
            pro_dev_addr = dev_addr;
            pro_mount_time = to_ms_since_boot(get_absolute_time());
            pro_init_attempted = false;
        }
    } else {
        printf("Switch: Failed to allocate controller\n");
    }
}

void switch_unmount_cb(uint8_t dev_addr) {
    usb_map_unregister_gamepad(dev_addr);
    printf("Switch controller unmount (addr=%d)\n", dev_addr);
    if (dev_addr == pro_dev_addr) {
        pro_needs_init = false;
        pro_init_attempted = false;
    }
    free_controller(dev_addr);

    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}

uint8_t switch_connected_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < controller_count; i++) {
        if (controllers[i].connected) count++;
    }
    return count;
}
