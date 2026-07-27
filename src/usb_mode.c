/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * USB role selection + persistence (see usb_mode.h).
 */

#include "config.h"
#include "usb_mode.h"

#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>

#include "platform/amiga/mouse_config.h"

static usb_mode_t s_mode = USB_MODE_HOST;

void usb_mode_init(void)
{
#if ENABLE_USB_DEVICE_MODE
    port_config_data_t cfg;
    port_config_load(&cfg);
    s_mode = cfg.usb_device_mode ? USB_MODE_DEVICE : USB_MODE_HOST;
#else
    s_mode = USB_MODE_HOST;
#endif
    printf("[USBMODE] Boot mode: %s\n", s_mode == USB_MODE_DEVICE ? "DEVICE (Amiga->PC)" : "HOST (USB->Amiga)");
}

usb_mode_t usb_mode_get(void)
{
    return s_mode;
}

bool usb_mode_is_device(void)
{
    return s_mode == USB_MODE_DEVICE;
}

void usb_mode_start_usb(void)
{
    tusb_rhport_init_t init = {
        .role  = (s_mode == USB_MODE_DEVICE) ? TUSB_ROLE_DEVICE : TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(0, &init);
}

void usb_mode_request_toggle(void)
{
#if ENABLE_USB_DEVICE_MODE
    port_config_data_t cfg;
    port_config_load(&cfg);
    uint8_t next = cfg.usb_device_mode ? 0 : 1;
    cfg.usb_device_mode = next;

    printf("[USBMODE] Toggle requested -> %s; saving and rebooting\n",
           next ? "DEVICE / Host Mode UI" : "HOST / Device Mode UI");

    // Must write immediately: a deferred BT-pause save would reboot before flush
    // and leave the previous role in flash (often looks like "stuck in Host Mode").
    if (!port_config_save_immediate(&cfg)) {
        printf("[USBMODE] ERROR: could not persist USB mode; staying in current mode\n");
        return;
    }

    // Verify flash before reboot so a failed/partial write cannot silently revert.
    port_config_data_t verify;
    port_config_load(&verify);
    if (verify.usb_device_mode != next) {
        printf("[USBMODE] ERROR: flash verify failed (got %u, want %u); not rebooting\n",
               (unsigned)verify.usb_device_mode, (unsigned)next);
        return;
    }

    sleep_ms(100);
    watchdog_reboot(0, 0, 0);
    while (1) { tight_loop_contents(); }
#endif
}
