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
#include <stdio.h>

#include "display/disp_ssd.h"
#include "platform/amiga/keyboard_serial_io.h"
#include "platform/amiga/quad_mouse.h"
#include "platform/amiga/joystick_port1.h"
#include "platform/amiga/joystick_port2.h"
#include "util/debug_cons.h"
#include "util/output.h"

#include "config.h"
#include "tusb_config.h"

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#endif

// defined within usb_hid.c
extern void hid_app_task(void);
#if ENABLE_BLUEPAD32
extern void process_bluepad32_devices(void);
#endif

// Software version - increment this with each build to verify latest firmware is loaded
#define SOFTWARE_VERSION_MAJOR 1
#define SOFTWARE_VERSION_MINOR 0
#define SOFTWARE_VERSION_PATCH 0
#define SOFTWARE_VERSION_BUILD 1

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();

    // Initialize UART for debug output (needed for version print)
    stdio_init_all();
    
    // Print version number at startup to verify build and serial output
    printf("\n");
    printf("========================================\n");
    printf("amigahid-pico v%d.%d.%d (build %d)\n", 
           SOFTWARE_VERSION_MAJOR, SOFTWARE_VERSION_MINOR, 
           SOFTWARE_VERSION_PATCH, SOFTWARE_VERSION_BUILD);
    printf("========================================\n");
    printf("Port 1 toggle: Shift + Left Amiga + J\n");
    printf("Llamatron mode: Shift + Left Amiga + L\n");
    printf("========================================\n\n");

    // initialise the i2c controller and send the init sequence to the display
    disp_ssd_init();

    // say hello, trevor ("hello, trevor")
    dbgcons_init();

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

    // initialize joystick port 1 (shares GPIO pins with mouse)
    amiga_joystick_port1_init();

    // initialize joystick port 2 (dedicated GPIO pins for Revision 5)
    amiga_joystick_port2_init();

#if ENABLE_BLUEPAD32
    // initialize bluepad32 for Bluetooth keyboard support (Pico 2 W only)
    bluepad32_init();
#endif

    while (1) {
        // run host mode jobs (hotplug events, packet io callbacks)
        tuh_task();

        // amiga keyboard service routine
        amiga_service();

#if ENABLE_BLUEPAD32
        // poll bluepad32 for Bluetooth events
        bluepad32_poll();
        
        // Optimized: batch process all Bluetooth devices with early returns
        // Only processes devices that are actually connected, reducing overhead
        process_bluepad32_devices();
#endif
    }

    return 0;
}
