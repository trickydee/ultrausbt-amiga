# Release notes

Firmware version source of truth: `SOFTWARE_VERSION_*` in [`src/config.h`](./src/config.h).

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
