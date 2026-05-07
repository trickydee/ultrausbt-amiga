# amigahid-pico

please note: the interesting stuff goes into the [development](https://github.com/borb/amigahid-pico/tree/development) branch, so if main seems slow, check it out.

## introduction

amigahid-pico uses the rp2040 microcontroller (e.g. the raspberry pi pico) in a carrier board to attach usb input devices to a standard amiga without the need for a usb stack on the amiga itself.

it currently supports keyboards and mice and provides connection via the internal keyboard connector, the rj11/din socket on big-box amigas, and via the controller ports.

**this project is very much a work in progress.**

### Supported USB controllers (Joystick Port 2)

USB gamepads are mapped to Amiga joystick port 2 (directions, fire, and second/third button where supported). The following have dedicated drivers:

| Controller | Notes |
|------------|--------|
| **Sony DualShock 3** (PS3) | Wired USB; also third‑party PS3‑compatible devices (HORI, Mad Catz, Qanba, Nacon, Logitech F310, Zero Delay encoder, etc.) using the same HID report format |
| **Sony DualShock 4** (PS4) | Wired USB; also third‑party PS4‑compatible devices (HORI, Razer, Brook, Mad Catz, Qanba, Nacon, PowerA, etc.) using the same HID report format |
| **Sony DualSense** (PS5) | Wired USB (report ID 0x01); DualSense and DualSense Edge supported |
| **Nintendo Switch Pro Controller** | Wired USB; Pro, JoyCon L/R/pair, JoyCon Charge Grip (0x200E), SNES Controller NSO (0x2017); compatible third‑party Switch-style pads (e.g. PowerA) also supported |
| **Sony PlayStation Classic** (PSC) | Wired USB (0x054C / 0x0CDA); D-pad and face/shoulder buttons mapped to port 2 |
| **HORI HORIPAD** (Switch) | Wired USB (0x0F0D / 0x00C1); D-pad or left stick, B/A/Y and shoulders mapped to port 2 |
| **Google Stadia** | Wired USB (Google Controller) |
| **Xbox** | XInput: Xbox 360 (wired/wireless dongle), Xbox One, OG Xbox; HID fallback for Xbox pads that enumerate as HID |

Other USB HID gamepads may work via the generic gamepad path (directions + up to 3 buttons). Bluetooth gamepads are supported on Pico W / Pico 2 W builds via Bluepad32.

## **important note**

* current kicad files are for r5 pcb, pin mappings are not yet in the source tree! i have not yet generated this board as a pcb, but it is mostly identical to r4. the keyboard and controller port 1 are correct at time of writing, and will be fixed for the second controller port when the next prototype arrives.

* **always read the [errata](./doc/errata.md) section for the current pcb layout before deciding whether or not to build.**

![rev 5 pcb](./doc/images/board-rev-5.jpg)

## documentation

* [installation](./doc/installation.md)
* [hardware](./doc/hardware.md)
* [errors in revisions (errata)](./doc/errata.md)

## history

this project is a rewrite of the [amigahid](https://github.com/borb/amigahid) project to the rp2040 microcontroller.

originally, after the retirement of the arduino adk board and shortening availability of the max3421e "usb host shield" for arduino boards, the need for a cheap, easy and continually available replacement was sought.

at first, the uhs mini via a level shifter seemed ideal, but significant issues were found convincing it to work with a variety of arduino boards.

the rp2040 made sense as a target because it is widely available, has sufficient physical connections and is powerful enough to meet multiple needs.

## what is the latency of this?

i have not measured the latency, but the keyboard signals are sent out the moment they are received on the usb bus. the potential latency is likely fractionally longer than the amiga mcu but bear in mind the rp2040 is significantly faster than the standard amiga keyboard controller.

## release notes

### Pairing UX and startup window (v2.1.0)
- **Bluetooth pairing control on splash**: Right button now toggles pairing ON/OFF directly from the splash screen.
- **Safer key reset flow**: Hold **Left + Right** for 5 seconds to clear stored pairing keys, with on-screen `Pairing Clear` countdown feedback.
- **Startup pairing window**: Pairing is automatically enabled for 60 seconds after boot, then disabled until manually toggled back on.
- **Live countdown refresh**: Splash screen pairing status updates once per second (for example `Pair ON 42s`) without needing to switch screens.

### More controllers and Switch PIDs (v1.0.54)
- **Switch**: Added JoyCon Charge Grip (0x200E) and SNES Controller NSO (0x2017) to the Switch driver; same report format as Pro Controller.
- **PlayStation Classic (PSC)**: New dedicated driver for Sony PS Classic controller (0x054C / 0x0CDA). D-pad, Cross/Circle/Square/Triangle, L1/R1/L2/R2 mapped to Amiga joystick port 2.
- **HORI HORIPAD**: New dedicated driver for HORI HORIPAD for Nintendo Switch (0x0F0D / 0x00C1). D-pad or left stick for direction; B/A/Y and R2 for fire/buttons.

### Controller support and version display (v1.0.53)
- **PS5 DualSense**: Added USB support for Sony DualSense and DualSense Edge (report ID 0x01, joypad-os compatible report layout). Left stick, D-pad, Cross/Circle/Square/Triangle, R2/L2 (analog and digital) mapped to Amiga joystick.
- **PS3 / PS4 third‑party devices**: Extended VID/PID lists to match joypad-os; arcade sticks and compatible pads (HORI, Mad Catz, Qanba, Razer, Brook, Nacon, Logitech F310, PowerA, etc.) that use the same HID report format are now recognised and work as PS3 or PS4.
- **Version display**: Firmware version is defined in `config.h` (single source of truth); serial startup output and OLED splash both show the same version (e.g. v1.0.53).

### Bluetooth Fix (revision5-wip branch)
- **Fixed Bluetooth pairing issue**: Added `pico_btstack_ble` library to linker dependencies
  - Provides GATT client functions required for HID service discovery
  - Fixes issue where Bluetooth keyboards would pair then immediately unpair
  - Required for proper Bluepad32 HID service discovery and connection maintenance

## roadmap

please see the issues tab on the [github repository](https://github.com/borb/amigahid-pico) for the current list of planned features. the tl;dr is:

normal:
* i<sup>2</sup>c display for config/status
* controller emulation
    * keyboard-based controller emulation (use udlr for directions?)
    * USB mouse to joystick port 1 emulation
* flash memory-based configuration
* possible amiga-side control panel (using bidirectional controller port signals)
* rotary control simulation (for controlling gotek drives)

crazy talk:
* non-amiga support
* non-hidbp support
* analogue/paddle emulation
* fix some tinyusb issues (hotplug, timing-related instability)

## license

on the fence at the moment, but the current license choice is Eclipse Public License 2.0 (EPL-2.0).

## third party licenses

* µgui: this code contains 'µgui' by achim döbler; the license for this can be read at in the [display readme](./src/display/README.md)

## whuh... who?

nine <[nine@aphlor.org](mailto:nine@aphlor.org)>
