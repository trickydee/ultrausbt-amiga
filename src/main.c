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
extern void process_bluepad32_keyboard(void);
extern void process_bluepad32_mouse(void);
extern void process_bluepad32_gamepad(void);

// main entry point
int main(void)
{
    // tinyusb board init; led, uart, button, usb
    board_init();

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
        
        // process Bluetooth keyboard events
        process_bluepad32_keyboard();
        
        // process Bluetooth mouse events
        process_bluepad32_mouse();
        
        // process Bluetooth gamepad events (first gamepad mapped to joystick port 2)
        process_bluepad32_gamepad();
#endif
    }

    return 0;
}
