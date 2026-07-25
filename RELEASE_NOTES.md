# Release notes

Firmware version source of truth: `SOFTWARE_VERSION_*` in [`src/config.h`](./src/config.h).

## v4.0.11

* Mouse: revert emit-on-pending (v4.0.10); back to period-gated quadrature emit with separate HID consume — preferred after A/B on Ami/Atr Ms

## v4.0.10

* Mouse: restore emit-on-pending (as well as on Amiga/Atari period) so HID bursts can drain faster — A/B for Amiga travel vs Atari edge rate

## v4.0.9

* Atari mouse: raise pulse queue max from 96 → 255 (match jjmz sat-255) so fast flicks drop less travel

## v4.0.8

* Mouse quadrature: fix direction-cancel on partial opposite HID — keep remaining pulses in the original direction (jjmz adapter semantics). Stops fast flicks from tracking then reversing, especially visible in slower Atari emit mode.

## v4.0.7

* Atari mouse: separate HID consume from quadrature emit; drop leftover recirculation (was forcing ~50 µs edges and a long coasting tail → lag/overrun); modest pending clamp (±255) and shorter Atari pulse queue (96)

## v4.0.6

* Atari/Amiga mouse: Core 0 pending X/Y widened to int16 (±1023) so fast HID bursts are not discarded at ±127; excess beyond Core 1’s pulse queue is kept in the backlog

## v4.0.5

* Atari mouse mode: Core 1 quadrature tick uses `ATARI_UPDATE_PERIOD_US` (~450 µs) instead of Amiga ~170 µs — should fix fast-flick “held back” cursor

## v4.0.4

* Splash heading **Controller Mode**; Port 1 above Port 2; labels `1:Ami Ms` / `2:Joy`
* Mouse mode names **Ami Ms** / **Atr Ms**

## v4.0.3

* Port 1 OLED cycle: **Ami Ms → Joy → CD32 → Llama → Atr Ms** (Amiga / Atari mouse in the mode list)
* Port 2 OLED cycle: **Joy ↔ CD32** (label was STD)

## v4.0.2

* Splash home screen shows both ports (`2: CD32` / `1: Mouse`) in large type
* OLED **Right** toggles Port 2 STD ↔ CD32; **Middle + Left** toggles Bluetooth pairing
* Version string moved to splash bottom-right (PAIR hint removed)

## v4.0.1

* Llamatron twin-stick works with Port 2 CD32 enabled (still exclusive with Port 1 CD32)
* In Llamatron + Port 2 CD32, Port 2 gets the full seven-button CD32 map

## v4.0.0

* Public release packaging for [ultrausbt-amiga](https://github.com/trickydee/ultrausbt-amiga)
* EPL-2.0 `LICENSE` + `NOTICE`; ultrausbt copyright; Cursor/LLM credit in Acknowledgements
* Quieter UART by default (boot + device connect kept; controller dump spam gated)
* Docs: public index, `doc/archive/` for historical notes, rewritten README

## v3.2.1

* Caps Lock pulse held ~120 ms so macOS accepts the toggle
* `KEYBOARD_IN_DEBUG` default off

## v3.2.0

* USB device mode: Amiga keyboard + Port 1 mouse → PC as HID
* OLED Middle + Right (2 s) toggles host ↔ device; persisted + reboot

## v3.1.0

* Alternate reset: Ctrl + Left Amiga + Backspace
* Core 1 mouse consume safe across Bluetooth flash lockout (Stadia)

## v2.2.x

* Dual Port 1 + Port 2 CD32; Map Devices UI; pairing UX

Earlier history: see git log.
