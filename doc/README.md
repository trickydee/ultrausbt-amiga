# ultrausbt-amiga documentation

Public docs for the Amiga USB/Bluetooth adapter firmware (Pico / Pico 2 / Pico 2 W).

## Start here

| Doc | Audience |
|-----|----------|
| [`../README.md`](../README.md) | Features, shortcuts, build & flash |
| [`architecture.md`](./architecture.md) | Platform architecture + developer/agent quickstart |
| [`../RELEASE_NOTES.md`](../RELEASE_NOTES.md) | Firmware changelog |
| [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md) | Why Rev 6 moved Port 2 / OLED pins |

## Features

| Doc | Topic |
|-----|--------|
| [`BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md) | Bluetooth pairing on Pico 2 W |
| [`host-mode-port2-joystick.md`](./host-mode-port2-joystick.md) | Research: Port 2 Atari / Mega Drive pads → USB (PC/MiSTer) |
| [`future_work.md`](./future_work.md) | Known limitations & roadmap |
| [`submodule-versions.md`](./submodule-versions.md) | Pinned SDK / Bluepad32 / TinyUSB versions |

## Legal

| File | Topic |
|------|--------|
| [`../LICENSE`](../LICENSE) | Eclipse Public License 2.0 |
| [`../NOTICE`](../NOTICE) | Copyright & upstream credit |

## Archive

Older investigations, build specs, and detailed troubleshooting live in [`archive/`](./archive/) (not required for day-to-day use):

| Doc | Topic |
|-----|--------|
| [`archive/stadia-controller-verification.md`](./archive/stadia-controller-verification.md) | Stadia / Core 1 mouse verification notes |
| [`archive/borb-amigahid-hardware.md`](./archive/borb-amigahid-hardware.md) | Upstream hardware notes (amigahid-pico / borb) |
| [`archive/amiga-code-atari-board-gpio-mappings.md`](./archive/amiga-code-atari-board-gpio-mappings.md) | Rev 5 / Rev 6 GPIO map (detail) |
| [`archive/borb-amigahid-errata.md`](./archive/borb-amigahid-errata.md) | Upstream PCB errata (amigahid-pico / borb) |
| [`archive/device_troubleshooting.md`](./archive/device_troubleshooting.md) | Device / BT troubleshooting detail |
| [`archive/amiga-usb-device-mode.md`](./archive/amiga-usb-device-mode.md) | Amiga keyboard/mouse → PC (protocol detail) |
| [`archive/CD32_BUILD_SPEC.md`](./archive/CD32_BUILD_SPEC.md) | CD32 implementation build spec |
