/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * https://github.com/borb/amigahid-pico
 *
 * Modifications Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * debug console routines.
 */

#include <stdint.h>
#include <stdio.h>

#include "debug_cons.h"
#include "output.h"

struct
{
    uint8_t hid_keyboard, hid_mouse, hid_controller;
    uint8_t plug_events, unplug_events;
} debug_counters;

void dbgcons_init()
{
    // Note: no VT_ED_CLS (screen clear) here — keep the power-on banner and all
    // boot messages printed before dbgcons_init() visible in the serial log.
    ahprintf(
        "\nultrausbt-amiga by ultrausbt, https://github.com/trickydee/ultrausbt-amiga\n"
        "based on amigahid-pico by nine <nine@aphlor.org>, https://github.com/borb/amigahid-pico\n"
    );

    debug_counters.hid_keyboard = 0;
    debug_counters.hid_mouse = 0;
    debug_counters.hid_controller = 0;
    debug_counters.plug_events = 0;
    debug_counters.unplug_events = 0;

    dbgcons_print_counters();
}

void dbgcons_print_counters()
{
    // Debug output removed - counters still tracked but not printed
    // Display output handled by display module
}

void dbgcons_plug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard++;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse++;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller++;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.plug_events++;
    dbgcons_print_counters();
}

void dbgcons_unplug(enum debug_plug_types devtype)
{
    switch (devtype) {
        case AP_H_KEYBOARD:
            debug_counters.hid_keyboard--;
            break;
        case AP_H_MOUSE:
            debug_counters.hid_mouse--;
            break;
        case AP_H_CONTROLLER:
            debug_counters.hid_controller--;
            break;
        case AP_H_UNKNOWN:
        default:
            break;
    }
    debug_counters.unplug_events++;
    dbgcons_print_counters();
}

void dbgcons_amiga_key(uint8_t incode, uint8_t outcode, char *updown)
{
    (void)incode;
    (void)outcode;
    (void)updown;
    // Debug output removed - display output handled by display module
}

void dbgcons_amiga_mod(uint8_t outcode, char updown)
{
    // ls rs cl ct la ra lam ram
}
