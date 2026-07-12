/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * main entry point for amigahid-pico.
 */

// these reside within the tinyusb sdk and are not part of this project source
#include "bsp/board.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "pico/time.h"  // For watchdog timing
#include <stdio.h>

#include "config.h"
#include "tusb_config.h"

#include "display/display.h"
#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "platform/amiga/joystick_port1.h"
#include "platform/amiga/joystick_port2.h"
#if HIDPICO_REV_ATARI_BOARD
#include "platform/amiga/cd32_pad.h"
#include "platform/amiga/port_mode.h"
#include "platform/amiga/mouse_config.h"
#endif
#include "platform/common/gpio_util.h"
#include "util/debug_cons.h"
#include "util/output.h"

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#include "bluepad32_platform.h"
#endif

// defined within usb_hid.c
extern void hid_app_task(void);
extern void switch_check_delayed_init(void);
#if ENABLE_BLUEPAD32
extern void process_bluepad32_devices(void);
#endif

// Software version is defined in config.h (single source for main, display, serial)

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();

#if HIDPICO_REV_ATARI_BOARD
    // CRITICAL: Initialize all Amiga GPIOs to INPUT (safe state) BEFORE connecting to Amiga
    // This prevents 5V back-feeding damage when Amiga is powered but Pico is not
    // We do this immediately after board_init() to ensure clean GPIO state before:
    // - CYW43 initialization (if board_init() didn't already do it)
    // - I2C display initialization
    // - USB initialization
    // - Bluetooth initialization
    
    // Wait for power to stabilize (especially important if Amiga is already powered)
    sleep_ms(100);
    
    // Clear all GPIO direction cache and reset all Amiga GPIOs to INPUT (inactive/high) state
    // This ensures clean state even if Amiga is already powered and pull-ups are active
    // All GPIOs are set to INPUT with pull-up enabled - this is the SAFE state
    // GPIOs will only be set to OUTPUT when actively driving signals LOW
    // NOTE: This initializes all Amiga joystick/mouse GPIOs:
    //   Port 1: GPIOs 10-14 (directions + fire), GPIOs 2-3 (buttons 2-3)
    //   Port 2: GPIOs 19-22 (directions); fire/B2/B3 per HIDPICO_REVISION (Rev 5: 26-28, Rev 6: 7/0/1)
    // It does NOT affect CYW43 SPI pins or other system GPIOs
    amiga_gpio_reset_all_to_input();
#endif

    // Initialize UART for debug output (needed for version print)
    stdio_init_all();
    
    // Print version number at startup to verify build and serial output
    printf("\n");
    printf("========================================\n");
    printf("amigahid-pico v%d.%d.%d (PCB rev %d)\n",
               SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR,
               SOFTWARE_VERSION_PATCH, HIDPICO_REVISION);
    printf("========================================\n");
    printf("Port 1 toggle: Shift + Left Amiga + J\n");
    printf("Llamatron mode: Shift + Left Amiga + L\n");
    printf("Port 2 CD32 mode: Shift + Left Amiga + C\n");
    printf("========================================\n\n");

    dbgcons_init();
    
#if HIDPICO_REV_ATARI_BOARD
    // Print GPIO reset confirmation (after dbgcons_init so it's visible after screen clear)
    printf("[GPIO] State cleared and reset to INPUT (before other init)\n");
    
    // Initialize the display (same order as Atari code: after dbgcons_init)
    // Splash screen is shown by default
    display_init();
#endif

    // initialise the usb host stack on the rhport from tusb_config.h
    // Note: tuh_init() is deprecated, using tusb_init() with proper structure
    tusb_rhport_init_t host_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUH_RHPORT, &host_init);

    // we're single arch right now, but in future this should hand off to whatever the
    // configured arch is
    amiga_init();

    // start amiga mouse emulation
    amiga_quad_mouse_init();

#if HIDPICO_REV_ATARI_BOARD
    // Refresh splash screen after mouse type is loaded from flash
    // This ensures the correct title (AMIGA/ATARI) is displayed
    display_show_splash();
#endif

    // initialize joystick port 1 (shares GPIO pins with mouse)
    amiga_joystick_port1_init();

    // initialize joystick port 2 (dedicated GPIO pins for Revision 5/6)
    amiga_joystick_port2_init();
#if HIDPICO_REV_ATARI_BOARD
    cd32_port1_init();
    cd32_port2_init();
    port_mode_init();
    printf("[PORT] Boot Port 1 mode: %s (joy_flag=%d cd32=%d)\n",
           port_mode_port1_label(),
           amiga_joystick_port1_is_joystick_mode() ? 1 : 0,
           cd32_port1_is_enabled() ? 1 : 0);
    display_show_splash();
#endif

#if ENABLE_BLUEPAD32
    // initialize bluepad32 for Bluetooth keyboard support (Pico 2 W only)
    bluepad32_init();
    
#if HIDPICO_REV_ATARI_BOARD
    // Refresh splash screen after Bluetooth is initialized
    // This ensures the correct mode (USB+BT) is displayed
    display_show_splash();
#endif
#endif

#if HIDPICO_REV_ATARI_BOARD
    // Watchdog: Check GPIO state periodically (every 5 seconds)
    // This is lightweight - only checks a sample of GPIOs to detect stuck states
    absolute_time_t last_watchdog_check = get_absolute_time();
    const uint32_t WATCHDOG_INTERVAL_MS = 5000;  // Check every 5 seconds
#endif

    while (1) {
        // run host mode jobs (hotplug events, packet io callbacks)
        tuh_task();
        switch_check_delayed_init();

        // amiga keyboard service routine
        amiga_service();

#if HIDPICO_REV_ATARI_BOARD
        cd32_service();
#endif

#if ENABLE_BLUEPAD32
        if (bluepad32_is_enabled()) {
            bluepad32_poll();
            bluepad32_pairing_tick();
            core1_bt_pause_watchdog_tick();
            core1_heartbeat_watchdog_tick();
            port_config_flush_pending();

            process_bluepad32_devices();
        }
#endif

#if HIDPICO_REV_ATARI_BOARD
        // Lightweight watchdog: Check GPIO state periodically (not every loop iteration)
        // This minimizes performance impact while still detecting stuck GPIO states
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(last_watchdog_check, now) >= (WATCHDOG_INTERVAL_MS * 1000)) {
            if (amiga_gpio_watchdog_check()) {
                printf("[WATCHDOG] GPIO state recovery performed\n");
            }
            last_watchdog_check = now;
        }
        
        // Handle display button presses
        display_handle_buttons();
        display_tick();
#endif
    }

    return 0;
}
