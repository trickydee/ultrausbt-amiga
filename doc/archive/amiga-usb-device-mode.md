# USB Device Mode — use an Amiga keyboard/mouse on a PC

This mode reverses the adapter: instead of reading USB/Bluetooth devices and driving
the Amiga, it **reads a real Amiga keyboard (and Port 1 mouse) and presents the adapter
to a host PC as a USB HID keyboard + mouse**. It lets you use an Amiga 2000 (or other
Amiga) keyboard on a modern computer.

It is modelled on the "ADB host mode" feature of the sibling
`ultramegausb-apple-adb` firmware.

## Modes

| Mode | USB role | Data flow | Use |
|------|----------|-----------|-----|
| **Host** (default) | USB host | USB/BT devices → Amiga | Normal adapter operation |
| **Device** (this feature) | USB device | Amiga keyboard/mouse → PC | Use an Amiga keyboard on a PC |

The RP2040/RP2350 has a single USB PHY, so only one role is active at a time. The mode
is stored in flash and selected at boot.

## Switching modes

Hold the **Middle + Right OLED buttons together for 2 seconds**. The screen shows the
mode you are about to switch to (`PC KBD` or `AMIGA`); after the hold, the setting is
saved to flash and the adapter **reboots into the new mode**.

> A reboot is used (rather than live USB re-init) so the host PC always gets a clean
> USB enumeration and neither I/O subsystem is left half-initialised. The UX is still a
> single OLED gesture. Live re-init could be added later if desired.

## Wiring (reading the keyboard)

Device mode reuses the existing keyboard GPIOs, but as **inputs**:

| Signal | GPIO | Direction in device mode |
|--------|------|--------------------------|
| `KBD_AMIGA_CLK` | 6 | Input (keyboard is the clock master); IRQ on falling edge |
| `KBD_AMIGA_DAT` | 5 | Input; briefly driven low (~85 µs) as the handshake ACK |

The Port 1 mouse (quadrature + buttons) is read on the usual Port 1 GPIOs
(`QM1_AMIGA_H/HQ/V/VQ` and fire/B2/B3).

> **Electrical note:** `KDAT` must be **bidirectional** — the keyboard drives it while
> transmitting data, and the adapter must drive it low for the handshake/ACK. `KCLK` is
> read-only (keyboard is always the clock master). On the ultramegausb Atari board
> (Rev 5) both directions work as wired; the RP2350 is 5V-tolerant on these pins. If a
> board used a fixed-direction (output-only toward the Amiga) buffer on `KDAT`, the
> handshake could not reach the keyboard and it would sit in its resync loop forever
> (see below).

## Protocol (receive side)

The Amiga keyboard is the clock master. For each bit it places the (active-low,
inverted) data on `KDAT`, then pulses `KCLK` low. The firmware:

1. Samples `KDAT` on each `KCLK` **falling** edge, MSB first, 8 bits per frame
   (a `1` bit = `KDAT` driven low).
2. Un-rotates the byte: the keyboard sends `rotate_left(keycode | (up << 7))`, so
   `keycode | (up<<7) = rotate_right(raw)`. Keycodes `0x00–0x67` are real keys; higher
   values are telemetry (init/term power, lost sync, reset warning) and are ignored.
3. Translates the Amiga keycode to a USB HID usage using a reverse map built by
   inverting the existing `mapHidToAmiga[]` table; the modifier block (`0x60–0x67`) is
   mapped to HID modifier bits; caps lock is emitted as a press/release pulse to keep
   the host's locking state aligned.
4. Assembles a 6-key rollover + modifier report and hands it to the USB device layer.

### Handshake and resync (critical)

After **every** frame the keyboard waits (up to ~143 ms) for the computer to pull
`KDAT` low for ≥85 µs as an acknowledge. If it does not get that ACK it decides it is
"out of sync" and enters a **resync loop**: it clocks out a **single bit**, waits again
for the handshake, and repeats. On the wire this looks like a steady **~7 `KCLK`
edges/second** (1000 ms ÷ 143 ms ≈ 7) that never stops — and no key ever registers.

The firmware handles this on two paths:

- **Per-frame ACK (ISR):** the moment the 8th bit of a frame arrives, the KCLK ISR
  immediately drives the `KDAT` handshake. Doing it in the ISR (not the main loop)
  keeps it well inside the keyboard's timeout even while USB / OLED work is running.
- **Resync recovery (task):** during resync only a *single* bit is clocked, so a full
  frame never assembles and the per-frame ACK never fires. `keyboard_host_in_task()`
  detects a partial frame that has stalled mid-byte (bits clocked, then quiet for
  >3 ms — the resync signature) and issues the handshake anyway. This walks the
  keyboard back into sync; it then emits `0xF9` ("last keycode bad / lost sync") and
  resumes normal transmission. Healthy frames clock all 8 bits in well under 3 ms, so
  this path never triggers during normal typing.

Without the resync recovery a keyboard that ever slips out of sync (e.g. missed the
very first power-up ACK) can never be recovered, even though reads look fine.

## Implementation

| File | Role |
|------|------|
| `src/usb_mode.c` / `.h` | HOST/DEVICE selection, flash persistence, reboot-to-switch, `tusb_init` role |
| `src/usb_hid_device.c` / `.h` | TinyUSB *device* stack: composite keyboard+mouse HID descriptor, `tud_*` callbacks, report drainer |
| `src/platform/amiga/keyboard_host_in.c` / `.h` | KCLK IRQ capture, decode, ISR handshake + resync recovery, reverse keycode map |
| `src/platform/amiga/mouse_host_in.c` / `.h` | Port 1 quadrature + button read → relative USB mouse reports |
| `src/tusb_config.h` | Enables `CFG_TUD_*` alongside `CFG_TUH_*` |
| `src/config.h` | `ENABLE_USB_DEVICE_MODE`, `USB_DEVICE_VID/PID`, `KEYBOARD_IN_DEBUG` |

The mode is persisted in the existing `port_config` flash sector
(`usb_device_mode` byte).

## Build flags

| Macro | Default | Meaning |
|-------|---------|---------|
| `ENABLE_USB_DEVICE_MODE` | 1 | Compile the device-mode feature in. Set 0 to build host-only. |
| `USB_DEVICE_VID` / `USB_DEVICE_PID` | `0x2E8A` / `0xAB1A` | HID identity presented to the PC |
| `KEYBOARD_IN_DEBUG` | 0 | Log received frames, decoded keycodes, a 1 Hz KCLK edge/line heartbeat, named telemetry codes, and resync handshakes on UART. Useful for diagnosing handshake/wiring issues. |

### Diagnosing with `KEYBOARD_IN_DEBUG`

Default is **off (`0`)** in `src/config.h`. Set it to `1`, rebuild, and watch UART
(115200) when debugging device-mode keyboard receive. With the flag on, the
once-per-second heartbeat is the fastest way to read the state of the link:

```
[kbd-in] KCLK edges=136 (+7/s) CLK=1 DAT=1
```

- **`+0/s` at idle** — healthy; the keyboard is silent when no key is pressed.
- **steady `+7/s` at idle** — the keyboard is stuck in resync (not being ACKed). If you
  *also* see `resync handshake` lines the firmware is driving the ACK but the keyboard
  can't see it → suspect the `KDAT` drive path (wiring / fixed-direction buffer).
- **edges climb only while typing**, with `raw=`/`amiga=` values that match the keys →
  working normally.

Also logged when enabled: per-frame `raw=` / `amiga=` decode, named telemetry codes
(`0xFD` initiate / `0xFE` terminate power-up, etc.), and periodic resync-handshake
notices.

## Status / testing notes

- **Verified working on real hardware** (Amiga 2000 keyboard → ultramegausb Rev 5 board
  → macOS host). The resync recovery above was the fix that made typing reliable.
- If mouse axes are mirrored, flip the sign in `mouse_host_in_task()`.
- Mouse quadrature is currently polled from the main loop; if fast motion is dropped,
  move the decode to a GPIO IRQ or PIO state machine.
- Joystick-to-PC (gamepad HID) is not implemented; it would add a third
  `TUD_HID_REPORT_DESC_GAMEPAD` interface + `tud_hid_gamepad_report()`.
