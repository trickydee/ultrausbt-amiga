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

**CD32 gamepad mode (Rev 5, v2.1.2):** Toggle Port 2 to the Amiga CD32 seven-button protocol with **Shift + Left Amiga + C** (Left Command / Left Windows on PC/Mac keyboards). Maps modern face buttons and shoulders to CD32 Blue/Red/Yellow/Green/FF/Rew/Pause for CD32-enhanced Amiga titles. See [`doc/CD32_BUILD_SPEC.md`](./doc/CD32_BUILD_SPEC.md).

## **important note**

* current kicad files are for r5 pcb, pin mappings are not yet in the source tree! i have not yet generated this board as a pcb, but it is mostly identical to r4. the keyboard and controller port 1 are correct at time of writing, and will be fixed for the second controller port when the next prototype arrives.

* **always read the [errata](./doc/errata.md) section for the current pcb layout before deciding whether or not to build.**

![rev 5 pcb](./doc/images/board-rev-5.jpg)

## documentation

* [continue here (session handoff)](./doc/CONTINUE_HERE.md) — start here when resuming development
* [hardware](./doc/hardware.md)
* [errors in revisions (errata)](./doc/errata.md)

## building

Firmware is built with **`build-all.sh`** (see `doc/CONTINUE_HERE.md`). Output UF2s land in **`dist/`**.

```bash
./build-all.sh                              # default: Pico 2 W
BUILD_BOARDS=pico,pico2_w ./build.sh        # Pico + Pico 2 W
```

## history

this project is a rewrite of the [amigahid](https://github.com/borb/amigahid) project to the rp2040 microcontroller.

originally, after the retirement of the arduino adk board and shortening availability of the max3421e "usb host shield" for arduino boards, the need for a cheap, easy and continually available replacement was sought.

at first, the uhs mini via a level shifter seemed ideal, but significant issues were found convincing it to work with a variety of arduino boards.

the rp2040 made sense as a target because it is widely available, has sufficient physical connections and is powerful enough to meet multiple needs.

## what is the latency of this?

i have not measured the latency, but the keyboard signals are sent out the moment they are received on the usb bus. the potential latency is likely fractionally longer than the amiga mcu but bear in mind the rp2040 is significantly faster than the standard amiga keyboard controller.

## release notes

### Dual CD32 pads + stability fixes (v2.2.2)
- **Dual CD32:** Independent per-port shift registers (Port 1 + Port 2 simultaneously).
- **Fix:** Lightweight CD32 ISRs + `cd32_service()` — restores OLED/UI and Amiga output (v2.2.1 regression).
- **Fix:** GPIO watchdog skipped while CD32 active.
- **Known instability:** Occasional ghost adjacent buttons (B+A, Y+G); BT pad routing breaks when one of two pads disconnects. See `doc/future_work.md`.

### Dual CD32 pads (v2.2.1)
- **Simultaneous Port 1 + Port 2 CD32:** Independent shift-register drivers — two real CD32-style pads at once.
- **Setup:** OLED left button → Port 1 CD32; **Shift + Left Amiga + C** → Port 2 CD32; pair two BT gamepads.
- **Routing:** BT pad #1 → Port 2, BT pad #2 → Port 1; USB pad still Port 2 only.
- **Mouse:** Unavailable on Port 1 while Port 1 CD32 active.

### Port 1 CD32, mode persistence, OLED cycle (v2.2.0)
- **Port 1 CD32:** Seven-button protocol on Port 1 (BT gamepad #2); Core 1 mouse paused while active.
- **OLED left button:** Cycles Port 1 **MOUSE → JOY → LLAMA → CD32 → MOUSE** on splash and Devices screens.
- **Flash persistence:** Port 1 mode and Port 2 CD32 setting saved across reboots (`port_config` sector).
- **Port mode manager:** `port_mode.c` centralizes mode apply (CD32 vs Llamatron).

### CD32 gamepad mode (v2.1.2)
- **Port 2 CD32 protocol:** Shift-register emulation on Rev 5 GPIOs (JOYMODE/CLOCK/DATA on pins 5/6/9) for seven-button CD32 games.
- **Toggle:** **Shift + Left Amiga + C** from USB or Bluetooth keyboard; OLED Devices screen shows `Port2: CD32` or `Port2: STD`.
- **Gamepad routing:** Unified `port2_gamepad_submit()` for USB (PS4, Xbox, Stadia, generic HID) and Bluetooth gamepad #1.
- **Llamatron:** Mutually exclusive with CD32 mode on either port.

### Map Devices UI and build alignment (v2.1.1)
- **Map Devices screen**: Renamed from Bluetooth Devices; shows J2/J1/K1/M1 with Bluetooth and USB device names.
- **USB device map**: New `usb_device_map` module; driver-specific labels (PS5, Switch, Xbox, etc.) without generic overwrite.
- **Build flow**: `build-all.sh` aligned with Atari adapter — builds under `build/build-<board>/`, artifacts in `dist/`.
- **Docs**: Removed outdated `installation.md` and stale µgui README note; updated `.gitignore`.

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

## whuh... who?

nine <[nine@aphlor.org](mailto:nine@aphlor.org)>
