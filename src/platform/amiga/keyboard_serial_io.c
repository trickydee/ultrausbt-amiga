/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * amiga keyboard serial interface.
 */

#include "config.h"
#include "keyboard_serial_io.h"
#include "keyboard.h"
#include "keyboard.pio.h" // generated at compile time
#include "platform/common/gpio_util.h"
#include "util/output.h"
#include "util/debug_cons.h"

#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "class/hid/hid.h"

enum _sync_state { IDLE, SYNC };
// don't optimise variables hit by the timer isr (timer callback?)
volatile enum _sync_state sync_state = IDLE;
volatile bool clock_timer_fired = false;

// caps lock will be read by the hid loop
bool caps_lock = false;

/*
// _.-._.-._ @todo i've not been doing sync correctly for sooooooo long; fix/remove? -._.-._.-
int64_t sync_timer_cb(alarm_id_t id, void *user_data)
{
    clock_timer_fired = true;
    _keyboard_gpio_set(KBD_AMIGA_DAT, LOW);
    sync_state = SYNC;
}
*/

uint8_t get_modifier_from_hid(hid_keyboard_modifier_bm_t modifier)
{
    const hid_to_amiga_modifier_t *mapping;

    mapping = mapHidModToAmiga;
    while (mapping->hid_modifier != 0UL) {
        if (modifier == mapping->hid_modifier) {
            return mapping->amiga_keycode;
        }
        mapping++;
    }

    return 0;
}

void amiga_init()
{
    // setup digital mode, direction and active high/low on /clk, /dat and /rst.
    gpio_init(KBD_AMIGA_DAT);
    gpio_init(KBD_AMIGA_CLK);
    gpio_init(KBD_AMIGA_RST);

    gpio_set_function(KBD_AMIGA_DAT, GPIO_FUNC_SIO);
    gpio_set_function(KBD_AMIGA_CLK, GPIO_FUNC_SIO);
    gpio_set_function(KBD_AMIGA_RST, GPIO_FUNC_SIO);

    // all pins are active low, meaning if /rst is current at 0, the amiga is held in reset.
    // rectify this by putting all pins in open drain. this should bring the amiga to boot.
    // Use optimized shared GPIO utility
    amiga_gpio_init_active_low(KBD_AMIGA_DAT, false);
    amiga_gpio_init_active_low(KBD_AMIGA_CLK, false);
    amiga_gpio_init_active_low(KBD_AMIGA_RST, false);

    // now the pins are setup, setup the timer callback to maintain keyboard comms in sync.
    // @todo add_alarm_in_ms() here

    // wait a full second then send initpower, pause 200ms and then termpower. unlike the amiga kbd 6502, we're
    // not doing anything during this time, so it's just so the computer is happy in the knowledge that we are
    // here.
    sleep_ms(1000);
    amiga_send(AMIGA_INITPOWER, false);
    sleep_ms(200);
    amiga_send(AMIGA_TERMPOWER, false);

    // fin.
}

bool amiga_caps_lock()
{
    return caps_lock;
}

void amiga_hid_send(uint8_t hidcode, bool up)
{
    if (mapHidToAmiga[hidcode] == AMIGA_UNKNOWN) {
        // ahprintf("[akb] cowardly refusing to send $ff to the amiga\n");
        return;
    }

    // Disabled keyboard logging for now (can be re-enabled if needed)
    // dbgcons_amiga_key(hidcode, mapHidToAmiga[hidcode], up ? "u" : "d");

    amiga_send(mapHidToAmiga[hidcode], up);
}

void amiga_hid_modifier(hid_keyboard_modifier_bm_t modifier, bool up)
{
    uint8_t amiga_code;
    amiga_code = get_modifier_from_hid(modifier);

    // Disabled keyboard logging for now (can be re-enabled if needed)
    // @todo indicate the modifier state in dbgcons, somehow
    // dbgcons_amiga_key(0, amiga_code, up ? "u" : "d");

    amiga_send(amiga_code, up);
}

void amiga_send(uint8_t keycode, bool up)
{
    uint8_t bit_position, bit_mask = 0x80, sendcode;
    static bool ctrl = false, lamiga = false, ramiga = false, in_reset = false;

    // we don't care about caps lock coming up; ignore it
    if ((keycode == AMIGA_CAPSLOCK) && up)
        return;

    // check for caps lock going down and toggle; rewrite the 'up' parameter
    if (keycode == AMIGA_CAPSLOCK) {
        // amiga caps lock is odd; when it's on, it sends down code but no up, and vice versa when it comes off
        up = caps_lock;
        caps_lock = !caps_lock;

        // ahprintf("[akb] caps lock %s\n", caps_lock ? "ON" : "OFF");
    }

    if (keycode == AMIGA_CTRL)
        ctrl = !up;
    if (keycode == AMIGA_LAMIGA)
        lamiga = !up;
    if (keycode == AMIGA_RAMIGA)
        ramiga = !up;

    if ((ctrl && lamiga && ramiga) && !in_reset) {
        in_reset = true;
        amiga_assert_reset();
    }

    if (in_reset && !(ctrl && lamiga && ramiga)) {
        in_reset = false;
        amiga_release_reset();
    }

    // copy input code, roll left, move msb to lsb
    sendcode = keycode | (up == true ? 0x80 : 0x00);
    sendcode <<= 1;
    if (up || (keycode & 0x80))
        sendcode |= 1;

    for (bit_position = 0; bit_position < 8; bit_position++) {
        if (sendcode & bit_mask)
            amiga_gpio_set_active_low(KBD_AMIGA_DAT, true);   // LOW = active
        else
            amiga_gpio_set_active_low(KBD_AMIGA_DAT, false); // HIGH = inactive

        // hold /dat for 20us before pulsing /clk, then wait 50us before next bit
        sleep_us(20);
        amiga_gpio_set_active_low(KBD_AMIGA_CLK, true);   // LOW = active (pulse)
        sleep_us(20);
        amiga_gpio_set_active_low(KBD_AMIGA_CLK, false);  // HIGH = inactive
        sleep_us(50); // @todo should be 20?

        // shift the bit pattern for next iteration
        bit_mask >>= 1;
    }

    // set /dat to input for 5ms to signal end of key
    amiga_gpio_set_active_low(KBD_AMIGA_DAT, false); // HIGH = inactive
    sleep_ms(5);

    // @todo we _should_ be checking that the amiga has acked the code by watching /dat
    // for a lwo pulse. according to adcd2.1, while the computer cannot detect
    // out-of-sync, the keyboard can by looking for the pulse, sending $f9 then the
    // repeated code.
}

void amiga_assert_reset()
{
    // ahprintf("[akb] *** RESET BEING ASSERTED ***\n");
    amiga_gpio_set_active_low(KBD_AMIGA_RST, true);  // LOW = active (reset asserted)
}

void amiga_release_reset()
{
    // ahprintf("[akb] *** RESET BEING RELEASED ***\n");
    amiga_gpio_set_active_low(KBD_AMIGA_RST, false); // HIGH = inactive (reset released)
}

void amiga_service()
{
    if ((sync_state == SYNC) && clock_timer_fired) {
        // @todo THIS IS WRONG
        amiga_gpio_set_active_low(KBD_AMIGA_RST, false); // HIGH = inactive
        sync_state = IDLE;
        clock_timer_fired = false;
    }
}
