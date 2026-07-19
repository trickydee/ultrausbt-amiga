/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * USB device names for the Map Devices OLED screen.
 */

#include "usb_device_map.h"
#include "display/display.h"
#include <string.h>

typedef struct {
    uint8_t dev_addr;
    char name[USB_MAP_NAME_LEN];
    bool used;
} usb_joy_slot_t;

static usb_joy_slot_t joy_slots[2];
static char keyboard_name[USB_MAP_NAME_LEN];
static bool keyboard_present;
static char mouse_name[USB_MAP_NAME_LEN];
static bool mouse_present;

static void copy_name(char* dst, size_t len, const char* src)
{
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

static void compact_joy_slots(void)
{
    if (!joy_slots[0].used && joy_slots[1].used) {
        joy_slots[0] = joy_slots[1];
        joy_slots[1].used = false;
        joy_slots[1].dev_addr = 0;
        joy_slots[1].name[0] = '\0';
    }
}

void usb_map_register_gamepad(uint8_t dev_addr, const char* name)
{
    if (!dev_addr) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            copy_name(joy_slots[i].name, sizeof(joy_slots[i].name), name);
            display_update_devices();
            return;
        }
    }
    for (int i = 0; i < 2; i++) {
        if (!joy_slots[i].used) {
            joy_slots[i].used = true;
            joy_slots[i].dev_addr = dev_addr;
            copy_name(joy_slots[i].name, sizeof(joy_slots[i].name), name);
            display_update_devices();
            return;
        }
    }
}

void usb_map_unregister_gamepad(uint8_t dev_addr)
{
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            joy_slots[i].used = false;
            joy_slots[i].dev_addr = 0;
            joy_slots[i].name[0] = '\0';
            compact_joy_slots();
            display_update_devices();
            return;
        }
    }
}

bool usb_map_gamepad_registered(uint8_t dev_addr)
{
    if (!dev_addr) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            return true;
        }
    }
    return false;
}

const char* usb_map_get_gamepad(int slot)
{
    if (slot < 0 || slot > 1 || !joy_slots[slot].used) {
        return NULL;
    }
    return joy_slots[slot].name;
}

void usb_map_set_keyboard(const char* name)
{
    keyboard_present = true;
    copy_name(keyboard_name, sizeof(keyboard_name), name);
    display_update_devices();
}

void usb_map_clear_keyboard(void)
{
    keyboard_present = false;
    keyboard_name[0] = '\0';
    display_update_devices();
}

const char* usb_map_get_keyboard(void)
{
    return keyboard_present ? keyboard_name : NULL;
}

void usb_map_set_mouse(const char* name)
{
    mouse_present = true;
    copy_name(mouse_name, sizeof(mouse_name), name);
    display_update_devices();
}

void usb_map_clear_mouse(void)
{
    mouse_present = false;
    mouse_name[0] = '\0';
    display_update_devices();
}

const char* usb_map_get_mouse(void)
{
    return mouse_present ? mouse_name : NULL;
}
