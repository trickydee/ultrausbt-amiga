/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Reverse keyboard path — receive side of the Amiga keyboard serial protocol.
 *
 * The real Amiga keyboard is the clock master. For each bit it places the (active
 * low, i.e. inverted) data on KDAT, then pulses KCLK low. We sample KDAT on the
 * KCLK falling edge, MSB first, 8 bits per frame. The transmitted byte is the
 * keycode left-rotated by one with the key up/down flag in bit 0; we undo that to
 * recover keycode + up/down. After each frame the computer must pull KDAT low for
 * ~85us as a handshake, else the keyboard eventually declares lost sync.
 */

#include "config.h"

#if ENABLE_USB_DEVICE_MODE

#include "keyboard_host_in.h"
#include "keyboard.h"
#include "usb_hid_device.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

#include <stdio.h>
#include <string.h>

// Handshake pulse: computer holds KDAT low to acknowledge a received frame.
#define KBD_IN_HANDSHAKE_US   85
// Reset a partial frame if the keyboard stops clocking mid-byte (ISR path).
#define KBD_IN_FRAME_TIMEOUT_US 5000
// If a partial frame stalls this long, treat it as the keyboard waiting for a
// handshake (resync) and ACK it. Well above a normal inter-bit gap (~60-100 us),
// well below the keyboard's ~143 ms give-up timeout.
#define KBD_IN_RESYNC_STALL_US  3000

// Reverse map: Amiga keycode (0x00-0x5f) -> USB HID usage. Modifiers (0x60-0x67)
// and telemetry codes are handled separately.
static uint8_t s_amiga_to_hid[0x68];

// HID keyboard state assembled for the host.
static uint8_t s_hid_mod = 0;
static uint8_t s_hid_keys[6] = { 0 };

// Frame capture (ISR producer / task consumer).
static volatile uint8_t s_rx_bits = 0;
static volatile uint8_t s_rx_count = 0;
static volatile uint32_t s_last_edge_us = 0;
static volatile uint32_t s_edge_count = 0;   // total KCLK falling edges seen (debug)

#define FRAME_RING_SIZE 32
static volatile uint8_t s_frames[FRAME_RING_SIZE];
static volatile uint8_t s_frame_head = 0;  // written by ISR
static volatile uint8_t s_frame_tail = 0;  // read by task

static uint8_t amiga_mod_to_hid_bit(uint8_t amiga_code)
{
    switch (amiga_code) {
        case AMIGA_LSHIFT: return KEYBOARD_MODIFIER_LEFTSHIFT;
        case AMIGA_RSHIFT: return KEYBOARD_MODIFIER_RIGHTSHIFT;
        case AMIGA_CTRL:   return KEYBOARD_MODIFIER_LEFTCTRL;
        case AMIGA_LALT:   return KEYBOARD_MODIFIER_LEFTALT;
        case AMIGA_RALT:   return KEYBOARD_MODIFIER_RIGHTALT;
        case AMIGA_LAMIGA: return KEYBOARD_MODIFIER_LEFTGUI;
        case AMIGA_RAMIGA: return KEYBOARD_MODIFIER_RIGHTGUI;
        default:           return 0;
    }
}

static void build_reverse_map(void)
{
    memset(s_amiga_to_hid, 0, sizeof(s_amiga_to_hid));
    // Invert the existing HID->Amiga table (first HID that maps to a given Amiga
    // code wins). Skip the modifier block (0x60-0x67) which is handled explicitly.
    for (unsigned hid = 0; hid < 256; hid++) {
        uint8_t a = mapHidToAmiga[hid];
        if (a == AMIGA_UNKNOWN) continue;
        if (a >= 0x68) continue;             // telemetry / out of range
        if (a >= AMIGA_LSHIFT && a <= AMIGA_RAMIGA) continue;  // modifiers
        if (s_amiga_to_hid[a] == 0) s_amiga_to_hid[a] = (uint8_t)hid;
    }
}

static void kbd_in_handshake(void)
{
    // Pull KDAT low as the acknowledge pulse the keyboard waits for after every
    // frame, then release back to input (pull-up returns it high). The keyboard
    // times out after ~143 ms and enters its resync loop if it never sees this, so
    // we issue it immediately from the ISR to avoid any main-loop latency.
    gpio_put(KBD_AMIGA_DAT, 0);
    gpio_set_dir(KBD_AMIGA_DAT, GPIO_OUT);
    busy_wait_us(KBD_IN_HANDSHAKE_US);
    gpio_set_dir(KBD_AMIGA_DAT, GPIO_IN);
}

static void kbd_clk_isr(uint gpio, uint32_t events)
{
    if (gpio != KBD_AMIGA_CLK) return;
    (void)events;

    s_edge_count++;

    uint32_t now = time_us_32();
    // Discard a stale partial frame before accumulating this bit.
    if (s_rx_count != 0 && (now - s_last_edge_us) > KBD_IN_FRAME_TIMEOUT_US) {
        s_rx_count = 0;
        s_rx_bits = 0;
    }
    s_last_edge_us = now;

    // KDAT is active-low: a driven-low line represents a 1 bit.
    uint8_t bit = gpio_get(KBD_AMIGA_DAT) ? 0 : 1;
    s_rx_bits = (uint8_t)((s_rx_bits << 1) | bit);
    if (++s_rx_count >= 8) {
        uint8_t next = (uint8_t)((s_frame_head + 1) % FRAME_RING_SIZE);
        if (next != s_frame_tail) {          // drop on overflow rather than block
            s_frames[s_frame_head] = s_rx_bits;
            s_frame_head = next;
        }
        s_rx_count = 0;
        s_rx_bits = 0;

        // Acknowledge immediately so the keyboard never times out into resync.
        kbd_in_handshake();
    }
}

static void push_key(uint8_t hid_key, bool down)
{
    if (hid_key == 0) return;
    if (down) {
        for (int i = 0; i < 6; i++) if (s_hid_keys[i] == hid_key) return;  // dedup
        for (int i = 0; i < 6; i++) if (s_hid_keys[i] == 0) { s_hid_keys[i] = hid_key; return; }
    } else {
        for (int i = 0; i < 6; i++) if (s_hid_keys[i] == hid_key) { s_hid_keys[i] = 0; break; }
        // compact so a full array recovers slots
        uint8_t packed[6] = { 0 };
        int n = 0;
        for (int i = 0; i < 6; i++) if (s_hid_keys[i]) packed[n++] = s_hid_keys[i];
        memcpy(s_hid_keys, packed, sizeof(s_hid_keys));
    }
}

static void handle_frame(uint8_t raw)
{
    // Undo the transmit rotate: sendcode = rotate_left(keycode | (up<<7)).
    uint8_t p = (uint8_t)((raw >> 1) | ((raw & 1) << 7));
    uint8_t keycode = p & 0x7f;
    bool up = (p & 0x80) != 0;

    if (keycode > 0x67) {
#if KEYBOARD_IN_DEBUG
        const char* name = "unknown";
        switch (raw) {
            case 0xFD: name = "initiate power-up stream"; break;
            case 0xFE: name = "terminate power-up stream"; break;
            case 0xF9: name = "last keycode bad (resend)"; break;
            case 0xFA: name = "kbd buffer overflow";       break;
            case 0xFC: name = "kbd selftest FAILED";       break;
            case 0xF8: name = "reserved 0xF8";             break;
        }
        printf("[kbd-in] telemetry raw=0x%02x (%s)\n", raw, name);
#endif
        return;  // telemetry (init/term power, lost sync, reset warning, unused)
    }

#if KEYBOARD_IN_DEBUG
    printf("[kbd-in] raw=0x%02x amiga=0x%02x %s\n", raw, keycode, up ? "up" : "down");
#endif

    if (keycode == AMIGA_CAPSLOCK) {
        // Amiga caps lock is a locking key (down when turning on, up when off).
        // Pulse the host caps-lock once per physical toggle to keep states aligned.
        usb_hid_device_pulse_caps_lock();
        return;
    }

    uint8_t mod_bit = amiga_mod_to_hid_bit(keycode);
    if (mod_bit) {
        if (up) s_hid_mod &= (uint8_t)~mod_bit;
        else    s_hid_mod |= mod_bit;
    } else {
        push_key(s_amiga_to_hid[keycode], !up);
    }

    usb_hid_device_send_keyboard(s_hid_mod, s_hid_keys);
}

void keyboard_host_in_init(void)
{
    build_reverse_map();

    // KCLK and KDAT as inputs with pull-ups (idle high). We only briefly drive KDAT
    // low for the handshake.
    gpio_init(KBD_AMIGA_CLK);
    gpio_set_function(KBD_AMIGA_CLK, GPIO_FUNC_SIO);
    gpio_set_dir(KBD_AMIGA_CLK, GPIO_IN);
    gpio_pull_up(KBD_AMIGA_CLK);

    gpio_init(KBD_AMIGA_DAT);
    gpio_set_function(KBD_AMIGA_DAT, GPIO_FUNC_SIO);
    gpio_set_dir(KBD_AMIGA_DAT, GPIO_IN);
    gpio_pull_up(KBD_AMIGA_DAT);
    gpio_put(KBD_AMIGA_DAT, 0);  // preloaded low value used when we flip to output

    s_frame_head = s_frame_tail = 0;
    s_rx_count = 0;
    s_rx_bits = 0;

    gpio_set_irq_enabled_with_callback(KBD_AMIGA_CLK, GPIO_IRQ_EDGE_FALL, true, &kbd_clk_isr);

    printf("[kbd-in] Amiga keyboard receive active (KCLK=%d KDAT=%d)\n",
           KBD_AMIGA_CLK, KBD_AMIGA_DAT);
}

void keyboard_host_in_task(void)
{
    while (s_frame_tail != s_frame_head) {
        uint8_t raw = s_frames[s_frame_tail];
        s_frame_tail = (uint8_t)((s_frame_tail + 1) % FRAME_RING_SIZE);
        // Handshake already issued from the ISR the instant the frame completed.
        handle_frame(raw);
    }

    // Resync recovery. When the keyboard loses sync it clocks out a single bit and
    // waits (up to ~143 ms) for a handshake before sending the next one, so a full
    // 8-bit frame never assembles and the ISR's per-frame handshake never fires. If
    // we see a partial frame that has stalled mid-byte, the keyboard is waiting on
    // us: acknowledge it and reset so the keyboard can walk back into sync (it will
    // emit 0xF9 "lost sync" and resume). Normal frames clock all 8 bits in well under
    // this window, so this never triggers during healthy transmission.
    {
        uint32_t ints = save_and_disable_interrupts();
        uint8_t cnt = s_rx_count;
        uint32_t last_edge = s_last_edge_us;
        restore_interrupts(ints);

        if (cnt > 0 && (time_us_32() - last_edge) > KBD_IN_RESYNC_STALL_US) {
            ints = save_and_disable_interrupts();
            s_rx_count = 0;
            s_rx_bits = 0;
            restore_interrupts(ints);
            kbd_in_handshake();
#if KEYBOARD_IN_DEBUG
            static uint32_t resync_count = 0;
            if ((++resync_count % 20) == 1) {
                printf("[kbd-in] resync handshake (partial=%u bits, #%lu)\n",
                       cnt, (unsigned long)resync_count);
            }
#endif
        }
    }

#if KEYBOARD_IN_DEBUG
    // Once per second, report KCLK edge activity and the resting line levels. This
    // distinguishes "no signal at all" (edges stuck at 0 -> wiring / unidirectional
    // level shifter) from "garbage frames" (edges climbing but data is idle/high).
    static uint32_t last_report_ms = 0;
    static uint32_t last_edge_count = 0;
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms - last_report_ms >= 1000) {
        uint32_t edges = s_edge_count;
        printf("[kbd-in] KCLK edges=%lu (+%lu/s) CLK=%d DAT=%d\n",
               (unsigned long)edges,
               (unsigned long)(edges - last_edge_count),
               gpio_get(KBD_AMIGA_CLK), gpio_get(KBD_AMIGA_DAT));
        last_edge_count = edges;
        last_report_ms = now_ms;
    }
#endif
}

#endif // ENABLE_USB_DEVICE_MODE
