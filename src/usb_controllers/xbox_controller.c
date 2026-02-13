/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Xbox (XInput) controller implementation.
 * Registers the XInput host driver and maps gamepad input to Amiga joystick port 2.
 */

#include "xbox_controller.h"
#include "config.h"
#include "display/display.h"
#include "platform/amiga/joystick_port2.h"
#include "xinput_host.h"
#include <stdio.h>

#if CFG_TUH_XINPUT

//--------------------------------------------------------------------
// Driver registration (override weak symbol in usbh.c)
//--------------------------------------------------------------------

usbh_class_driver_t const *usbh_app_driver_get_cb(uint8_t *driver_count)
{
    extern usbh_class_driver_t const usbh_xinput_driver;
    *driver_count = 1;
    return &usbh_xinput_driver;
}

//--------------------------------------------------------------------
// Xbox controller state (first connected drives port 2)
//--------------------------------------------------------------------

#define XBOX_DEADZONE 8000  /* XInput stick range ±32768 */

static void xbox_update_amiga_joystick(uint8_t dev_addr, uint8_t instance,
                                       xinputh_interface_t const *xid_itf)
{
    xinput_gamepad_t const *pad = &xid_itf->pad;

    /* Direction: D-pad or left stick (with deadzone) */
    uint8_t direction = 0;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)    direction |= 0x01;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  direction |= 0x02;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  direction |= 0x04;
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) direction |= 0x08;

    if (direction == 0)
    {
        if (pad->sThumbLX < -XBOX_DEADZONE) direction |= 0x04;
        if (pad->sThumbLX > XBOX_DEADZONE)  direction |= 0x08;
        if (pad->sThumbLY > XBOX_DEADZONE)  direction |= 0x01;  /* Y up = positive */
        if (pad->sThumbLY < -XBOX_DEADZONE) direction |= 0x02;
    }

    amiga_joystick_port2_set_direction(AJ2_UP,    (direction & 0x01) != 0);
    amiga_joystick_port2_set_direction(AJ2_DOWN,  (direction & 0x02) != 0);
    amiga_joystick_port2_set_direction(AJ2_LEFT,  (direction & 0x04) != 0);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, (direction & 0x08) != 0);

    /* Buttons: A = fire, B = button2, X/Y = button3; right trigger can also fire */
    bool fire    = (pad->wButtons & XINPUT_GAMEPAD_A) != 0 || pad->bRightTrigger > 128;
    bool button2 = (pad->wButtons & XINPUT_GAMEPAD_B) != 0;
    bool button3 = (pad->wButtons & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y)) != 0;

    amiga_joystick_port2_set_button(AJ2_FIRE, fire);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, button2);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, button3);
}

static void xbox_clear_amiga_joystick(void)
{
    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}

//--------------------------------------------------------------------
// XInput callbacks
//--------------------------------------------------------------------

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf)
{
    const char *type_str;
    switch (xinput_itf->type)
    {
        case XBOX360_WIRED:     type_str = "Xbox 360 Wired"; break;
        case XBOX360_WIRELESS:  type_str = "Xbox 360 Wireless"; break;
        case XBOXONE:           type_str = "Xbox One"; break;
        case XBOXOG:            type_str = "Xbox OG"; break;
        default:                type_str = "Unknown"; break;
    }
    printf("Xbox controller mounted: %s (addr=%u, inst=%u)\n", type_str, (unsigned)dev_addr, (unsigned)instance);

#if HIDPICO_REVISION == 5
    display_show_controller_detected("Xbox", type_str, 3000);
#endif

    /* 360 Wireless: may not be connected yet; wait for first report */
    if (xinput_itf->type == XBOX360_WIRELESS && !xinput_itf->connected)
    {
        tuh_xinput_receive_report(dev_addr, instance);
        return;
    }

    tuh_xinput_set_led(dev_addr, instance, 0, true);
    tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)instance;
    printf("Xbox controller unmounted: addr=%u\n", (unsigned)dev_addr);
    xbox_clear_amiga_joystick();
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                    xinputh_interface_t const *xid_itf, uint16_t len)
{
    (void)len;
    /* Only drive joystick if controller is connected (wireless 360 sets this when synced) */
    if (xid_itf->connected)
    {
        xbox_update_amiga_joystick(dev_addr, instance, xid_itf);
    }
    tuh_xinput_receive_report(dev_addr, instance);
}

//--------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------

uint8_t xbox_connected_count(void)
{
    /* Count is maintained by the xinput host driver; we don't have a global list here.
     * Caller can use this for display; for now return 0/1 is not accurate without
     * tracking mounted devices. Leave as placeholder. */
    return 0;
}

#endif /* CFG_TUH_XINPUT */

//--------------------------------------------------------------------
// Xbox HID fallback (pads that expose HID instead of XInput interface)
//--------------------------------------------------------------------

#define XBOX_VID 0x045E

static bool xbox_pid_known(uint16_t pid)
{
    switch (pid) {
        case 0x028E: /* Xbox 360 Wired */
        case 0x0719: /* Xbox 360 Wireless */
        case 0x02A1: /* Xbox 360 Wireless (other) */
        case 0x02D1: /* Xbox One */
        case 0x02DD: /* Xbox One (firmware) */
        case 0x02EA: /* Xbox One S */
        case 0x0B00: /* Xbox One S (Bluetooth) / Series */
        case 0x0B05: /* Xbox Series X|S */
        case 0x0B0C: /* Xbox Series X|S (other) */
        case 0x0B12: /* Xbox Adaptive */
        case 0x0B13: /* Xbox Series (other) */
        case 0x0B20: /* Xbox Series (other) */
        case 0x0B22: /* Xbox Series (other) */
            return true;
        default:
            return false;
    }
}

bool xbox_is_hid_controller(uint16_t vid, uint16_t pid)
{
    return (vid == XBOX_VID && xbox_pid_known(pid));
}

void xbox_hid_mount_cb(uint8_t dev_addr)
{
    (void)dev_addr;
    printf("Xbox controller (HID) mounted\n");
#if HIDPICO_REVISION == 5
    display_show_controller_detected("Xbox", "HID", 3000);
#endif
}

void xbox_hid_umount_cb(uint8_t dev_addr)
{
    (void)dev_addr;
    printf("Xbox controller (HID) unmounted\n");
    amiga_joystick_port2_set_direction(AJ2_UP, false);
    amiga_joystick_port2_set_direction(AJ2_DOWN, false);
    amiga_joystick_port2_set_direction(AJ2_LEFT, false);
    amiga_joystick_port2_set_direction(AJ2_RIGHT, false);
    amiga_joystick_port2_set_button(AJ2_FIRE, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON2, false);
    amiga_joystick_port2_set_button(AJ2_BUTTON3, false);
}
