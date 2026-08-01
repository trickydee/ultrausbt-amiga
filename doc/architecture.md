# ultrausbt-amiga Architecture

This document describes the core platform architecture, how USB and Bluetooth integrate with the adapter, how to add USB controllers, how Bluepad32 is wired in, and how signaling is performed toward the Amiga.

## 1) System Overview

`ultrausbt-amiga` runs on RP2040/RP2350-based Pico hardware and supports two runtime USB roles:

- `USB_MODE_HOST` (normal adapter path): USB/Bluetooth input devices are translated into Amiga keyboard/mouse/joystick/CD32 electrical signaling.
- `USB_MODE_DEVICE` (reverse path / UI **Host Mode**): real Amiga keyboard, Port 1 mouse, and Port 2 Atari stick are read and re-exposed as USB HID keyboard + mouse + gamepad to a modern host (PC/Mac/Linux/MiSTer).

The role is persisted in flash and selected at boot.

## 2) Core Components

### Boot + orchestration

- `src/main.c`
  - Performs board and GPIO-safe initialization.
  - Loads persisted USB mode via `usb_mode_init()`.
  - Starts TinyUSB in host or device role via `usb_mode_start_usb()`.
  - In host mode initializes Amiga output subsystems and (optionally) Bluepad32.
  - Runs the main event loop dispatching USB tasks, Amiga services, Bluetooth polling, and OLED UI.

### USB role control and persistence

- `src/usb_mode.c`, `src/usb_mode.h`
  - Stores active role in `usb_mode_t`.
  - Reads persisted config from flash through port config storage.
  - Handles role toggle requests (`usb_mode_request_toggle()`), immediate flash write, verify, and reboot.
- `src/platform/amiga/mouse_config.c`
  - Persists config blob (mouse type, port modes, CD32 mode, USB role).
  - Supports deferred writes during BT critical sections and explicit immediate writes for USB-role toggles.

### USB host ingestion + routing

- `src/usb_hid.c`
  - TinyUSB host HID callbacks:
    - mount/unmount: device classification and registration
    - report receive: protocol/vendor-specific parser routing
  - Routes keyboard reports to Amiga keyboard serializer.
  - Routes mouse reports to:
    - quadrature mouse output, or
    - joystick conversion (when Port 1 is in joystick mode).
  - Routes gamepad data to joystick/CD32 submission paths.
- `src/usb_controllers/*.c`
  - Vendor-specific handlers for PS3/PS4/PS5/Stadia/Switch/Xbox/Hori/PSC.
  - Each module normalizes controller reports and submits canonical direction/button state.

### Bluetooth integration (Bluepad32 + BTstack + CYW43)

- `src/bluepad32_init.c`
  - Brings up async context, CYW43, BTstack run loop, and Bluepad32 core (`uni_init`).
- `src/bluepad32_platform.c`
  - Custom Bluepad32 platform callbacks for discovery/connect/ready/disconnect/data.
  - Tracks multiple BT keyboards/mice/gamepads and exposes accessor functions.
  - Manages pairing window and pairing on/off lifecycle.
  - Contains Core 1 pause/resume coordination for gamepad enumeration edge-cases.
- `src/usb_hid.c` (`process_bluepad32_*`)
  - Converts Bluepad32 keyboard/mouse/gamepad payloads into the same Amiga-facing handlers used by USB host input.

### Amiga signaling outputs / inputs

- Keyboard out: `src/platform/amiga/keyboard_serial_io.c`
- Mouse quadrature out: `src/platform/amiga/quad_mouse.c`
- Joystick Port 1 out: `src/platform/amiga/joystick_port1.c`
- Joystick Port 2 out: `src/platform/amiga/joystick_port2.c`
- Mode manager: `src/platform/amiga/port_mode.c`
- Reverse keyboard in (Amiga -> USB): `src/platform/amiga/keyboard_host_in.c`
- Reverse mouse in (Amiga -> USB): `src/platform/amiga/mouse_host_in.c`
- Reverse Port 2 stick in (Atari -> USB gamepad): `src/platform/amiga/joystick_port2_host_in.c`

### UI + device map

- `src/display/display.c`
  - Splash/device/settings screens, mode switches, pairing control.
- `src/usb_device_map.c`
  - Maintains USB device name slots used by OLED map/devices views.

## 3) End-to-End Data Paths

### Host mode (USB/BT -> Amiga)

1. TinyUSB host enumerates device (`tuh_task()` loop).
2. `usb_hid.c` receives reports and dispatches:
   - keyboard -> `amiga_hid_send()` path
   - mouse -> quadrature or Port1 joystick conversion
   - gamepad -> canonical submit functions (port gamepad/CD32 paths)
3. Bluepad32 device data is polled and fed into the same logical handlers.
4. Amiga GPIO lines are driven active-low with open-drain-safe behavior.

### Device mode (Amiga -> USB HID) — UI name **Host Mode**

1. TinyUSB device stack (`tud_task()`) enumerates as HID keyboard+mouse (IF0) + gamepad (IF1).
2. Amiga keyboard serial receive ISR/task reconstructs keycodes and modifiers.
3. Amiga quadrature mouse polling decodes movement/button state.
4. Port 2 Atari stick polling (`joystick_port2_host_in_task`) builds digital axes/hat/buttons.
5. `usb_hid_device_task()` publishes HID reports to the USB host.

## 4) USB + Bluetooth Integration Model

USB and Bluetooth are not separate adapters; they feed a shared internal input normalization layer:

- USB host devices arrive through TinyUSB host callbacks.
- Bluetooth devices arrive through Bluepad32 platform callbacks and accessors.
- Both are converted into common logical actions (key down/up, relative mouse delta, directional/button bitfields).
- Common Amiga output modules consume those actions and emit electrical signals.

Concurrency notes:

- Core 0 runs USB task loop, Bluepad32 polling, UI, and high-level routing.
- Core 1 runs high-frequency quadrature synthesis loop for smooth mouse output.
- Flash writes and BT discovery are coordinated with Core 1 pause/watchdog logic to avoid lockups.

## 5) How USB Controllers Are Added

### Existing pattern

Controller modules in `src/usb_controllers/` follow a standard shape:

1. `*_is_controller(vid,pid)` or equivalent matcher.
2. `*_mount_cb(dev_addr)` and `*_unmount_cb(dev_addr)` lifecycle hooks.
3. `*_process_report(dev_addr, report, len)` parser/translator.
4. Submit normalized output to shared Amiga-facing calls:
   - `port2_gamepad_submit(...)`, `port1_gamepad_submit(...)`, or
   - direct joystick set_direction/set_button where appropriate.

The dispatcher wiring happens in `src/usb_hid.c`:

- include module header
- add VID/PID detection in mount/unmount/report callbacks
- invoke module callbacks

### Recommended implementation steps

1. Add new files:
   - `src/usb_controllers/<name>_controller.h`
   - `src/usb_controllers/<name>_controller.c`
2. Define VID/PID matcher and report struct/layout parser.
3. Map source controls to adapter-neutral outputs (`dir_bits`, fire/B2/B3, optional CD32 shoulders/start).
4. Register/unregister OLED device name via `usb_map_register_gamepad()`/`usb_map_unregister_gamepad()`.
5. Wire module into `usb_hid.c` mount/unmount/report routing.
6. Validate in both normal joystick and CD32 mode behavior.

## 6) Bluepad32 Integration Details

Bluepad32 is integrated as a custom platform layer:

- Initialization:
  - build async context
  - set CYW43 async context before chip init
  - initialize CYW43 + BTstack run loop
  - set custom `uni_platform` handlers
  - call `uni_init()`
- Runtime:
  - `bluepad32_poll()` pumps BTstack.
  - `my_platform_on_controller_data()` captures latest keyboard/mouse/gamepad payloads.
  - `process_bluepad32_devices()` in `usb_hid.c` translates data into existing USB-style handlers.
- Pairing:
  - controlled by `bluepad32_pairing_start/stop/tick`
  - boot pairing window timeout logic
  - key deletion support for re-pair workflows
- Robustness:
  - Core 1 pause/watchdog/resume mechanisms for problematic BT discovery/reconnect cases.

## 7) How Signaling to the Amiga Is Performed

All primary Amiga-facing outputs are active-low GPIO semantics (line driven low = asserted), with safe initialization to input/high state to avoid back-feeding.

### Keyboard signaling (Amiga keyboard serial protocol)

- Module: `keyboard_serial_io.c`
- Uses `/DAT`, `/CLK`, `/RST` lines.
- On key transitions, HID->Amiga translation produces Amiga scan code events.
- Transmit encodes key up/down into 8-bit framed codes and bit-bangs:
  - set DAT bit
  - pulse CLK
  - timing delays per bit.
- Supports hard reset chord behavior by asserting reset-related lines.

### Mouse signaling (quadrature)

- Module: `quad_mouse.c`
- Core 0 accumulates HID motion deltas into atomic backlog.
- Core 1 consumes backlog, applies queue/direction handling, and emits phased quadrature waveforms on H/HQ and V/VQ.
- Update rates differ for Amiga vs Atari timing profiles.
- Port 1 joystick/CD32 modes gate or pause quadrature output as needed.

### Joystick/CD32 signaling

- Modules: `joystick_port1.c`, `joystick_port2.c`, `cd32_pad.c`, `port*_gamepad.c`
- Direction and button states are translated to active-low GPIO assertions.
- CD32 modes reuse canonical submissions but emit broader button sets where supported.

### Reverse signaling (Amiga -> USB mode)

- Keyboard input:
  - ISR on keyboard clock falling edge samples DAT bits.
  - Reconstructs Amiga frame -> HID key/modifier state.
  - Sends via `usb_hid_device_send_keyboard()`.
- Mouse input:
  - Poll quadrature states, decode Gray-code transitions into deltas.
  - Emit HID mouse reports via `usb_hid_device_send_mouse()`.
- Port 2 Atari stick input:
  - Poll dirs/fire/B2 as inputs with pull-ups (`joystick_port2_host_in.c`).
  - Emit HID gamepad on IF1 via `usb_hid_device_send_gamepad(x, y, hat, buttons)`.
  - Dirs on Axis 0/1 + hat; fire = B0; do **not** use buttons 12–15 (Mode clash on MiSTer).
  - Detail: [`host-mode-port2-joystick.md`](./host-mode-port2-joystick.md).

## 8) Architecture Diagrams

### 8.1 System context (hardware interfaces)

```text
                           +----------------------------------+
                           |          RP2040 / RP2350         |
                           |                                  |
                           |  Core 0                          |
 USB Host Port <---------->|  - TinyUSB host/device task      |
 (or USB Device to PC)     |  - Input routing / mode logic    |
                           |  - OLED UI / config persistence  |
                           |  - Bluepad32 polling             |
                           |                                  |
 Bluetooth (CYW43) <------>|  Bluepad32 + BTstack platform    |
                           |                                  |
                           |  Core 1                          |
                           |  - Quadrature mouse waveform gen |
                           +----------------+-----------------+
                                            |
                                            | Active-low GPIO signals
                                            v
      +---------------------+----------------------+----------------------+
      |                     |                      |                      |
  Amiga Keyboard        Amiga Port 1          Amiga Port 2          /KBRST
  /DAT /CLK /RST        Mouse/Joystick/CD32   Joystick/CD32         reset control
  bit-banged serial     quadrature + buttons  directions + buttons
```

### 8.2 Software block diagram (host mode: USB/BT → Amiga)

Shows module layers and the shared normalization path. Arrows are logical data flow.

```text
+==========================================================================+
|                              main.c                                      |
|  boot → usb_mode_init / start_usb → init Amiga outs (+ BT) → main loop   |
+==========================================================================+
          |                              |                         |
          v                              v                         v
+---------------------+    +---------------------------+   +----------------+
| TinyUSB Host        |    | Bluepad32 stack           |   | display.c      |
| tuh_task()          |    | bluepad32_init.c          |   | OLED screens / |
|                     |    | bluepad32_platform.c      |   | buttons / pair |
| usb_hid.c           |    |   slots: kb / mouse / pad |   +--------+-------+
|  mount / umount /   |    |   pairing + accessors     |            |
|  report callbacks   |    +-------------+-------------+            |
+----------+----------+                  |                          |
           |                             |                          |
           |  vendor pads                |  process_bluepad32_*     |
           v                             v                          |
+---------------------+    +---------------------------+            |
| usb_controllers/*   |    | Shared handlers in        |            |
| PS3/4/5 Stadia      |--->| usb_hid.c                 |<-----------+
| Switch Xbox Hori…   |    |  handle_event_keyboard    |  (counts /
+---------------------+    |  handle_event_mouse       |   map UI)
                           |  process_bluepad32_gamepad|
                           +-------------+-------------+
                                         |
                    normalized events    |
         (HID keycodes, mouse dx/dy,     |
          dir_bits + face/shoulder/start)|
                                         v
              +--------------------------+--------------------------+
              |              Amiga output layer                     |
              |                                                     |
              |  keyboard_serial_io.c   amiga_hid_send / bit-bang   |
              |  quad_mouse.c           Core0 feed → Core1 waves    |
              |  joystick_port1.c       Port1 dirs / buttons        |
              |  joystick_port2.c       Port2 dirs / buttons        |
              |  port1/2_gamepad.c      classic vs CD32 submit      |
              |  cd32_pad.c             CD32 shift protocol         |
              |  port_mode.c            mouse/joy/CD32/Llama state  |
              +--------------------------+--------------------------+
                                         |
                                         v
                                    Amiga GPIO
```

### 8.3 Software block diagram (device mode: Amiga → PC)

```text
+==========================================================================+
|                              main.c                                      |
|  usb_mode = DEVICE → kbd/mouse/joy2 host_in → tud_task loop              |
+==========================================================================+
     |                        |                         |              |
     v                        v                         v              v
+----------------+  +------------------+  +------------------------+ +-----------+
| keyboard_      |  | mouse_host_in.c  |  | joystick_port2_       | | display.c |
| host_in.c      |  | poll quad+btns   |  | host_in.c             | | Splash↔   |
| KCLK → HID kbd |  | → HID mouse      |  | Atari dirs/fire/B2    | | Settings  |
+-------+--------+  +--------+---------+  | → HID gamepad         | +-----------+
        |                    |            +-----------+------------+
        |                    |                        |
        |  usb_hid_device_send_*                      |
        v                    v                        v
        +--------------------+------------------------+
                             |
                             v
                  +---------------------------+
                  | usb_hid_device.c           |
                  | IF0: kbd + mouse          |
                  | IF1: gamepad (macOS/Chrome|
                  |      need separate IF)    |
                  +-------------+-------------+
                                |
                                v
                           USB to PC / MiSTer
```

Cross-cutting (both modes):

```text
+------------------+     +--------------------+     +------------------+
| usb_mode.c       |---->| mouse_config.c     |<----| port_mode.c      |
| role select /    |     | flash: mouse type, |     | apply Port1/2    |
| reboot toggle    |     | ports, USB role    |     | persisted modes  |
+------------------+     +--------------------+     +------------------+
```

## 9) Key Design Characteristics

- Single codebase supports both adapter direction modes through runtime USB role selection.
- USB and Bluetooth share normalization paths to reduce duplicate logic.
- Timing-sensitive mouse quadrature is isolated on Core 1.
- Flash persistence is coordinated with BT/Core1 constraints for runtime stability.
- Port-mode abstraction allows runtime behavior changes (mouse, joystick, Atari mapping, CD32, Llamatron).

## 10) Developer / Agent Quickstart

Use this section when onboarding a human developer or an AI agent into the codebase.

### Orientation (5 minutes)

| Question | Answer |
|----------|--------|
| Firmware version | `SOFTWARE_VERSION_*` in `src/config.h` (also mirrored in `README.md`, `RELEASE_NOTES.md`) |
| Board revision | `HIDPICO_REVISION` in root `CMakeLists.txt` (`5` or `6`) |
| Default USB role | Host = USB/BT → Amiga; Device = Amiga → PC HID |
| Main loop | `src/main.c` |
| USB report dispatch | `src/usb_hid.c` |
| BT platform | `src/bluepad32_platform.c` + `src/bluepad32_init.c` |
| Amiga outputs | `src/platform/amiga/` |
| OLED UI | `src/display/display.c` |
| Docs index | `doc/README.md` |

**Do not reinvent:** USB and BT inputs both normalize into the same Amiga-facing APIs (`amiga_hid_send`, `amiga_quad_mouse_*`, `port2_gamepad_submit`, etc.). Prefer extending an existing controller module pattern over inventing a parallel path.

### Build & flash

```bash
# Default Pico 2 W → dist/
./build-all.sh

# Incremental
CLEAN_BUILD_DIRS=0 ./build-all.sh
```

Flash the matching `.uf2` from `dist/` (BOOTSEL → RPI-RP2). Serial console shows boot banner and plug/unplug by default.

Debug toggles:

- `DEBUG_MESSAGES=1` via CMake compile definitions (DIAG / ahprintf)
- `CONTROLLER_DEBUG` / `KEYBOARD_IN_DEBUG` in `src/config.h`

### Agent “edit map” — where to change what

| Goal | Primary files |
|------|----------------|
| Add USB gamepad | `src/usb_controllers/<name>_controller.{c,h}`, `src/CMakeLists.txt`, `src/usb_hid.c` |
| Fix mouse feel / timing | `src/platform/amiga/quad_mouse.c` |
| Keyboard HID→Amiga map | `src/platform/amiga/keyboard.h` + `keyboard_serial_io.c` |
| Port modes (mouse/joy/CD32/Llama) | `src/platform/amiga/port_mode.c` |
| USB host↔device role | `src/usb_mode.c`, `mouse_config.c`, OLED Settings in `display.c` |
| Host Mode Port 2 stick → PC | `joystick_port2_host_in.c`, `usb_hid_device.c` (`CFG_TUD_HID` = 2) |
| BT pairing / device slots | `src/bluepad32_platform.c` |
| OLED screens / buttons | `src/display/display.c` |
| GPIO pin map | `src/config.h` (`HIDPICO_REVISION`) |
| Version bump | `src/config.h`, then `README.md`, `RELEASE_NOTES.md`, `doc/future_work.md` |

### Checklist: add a USB controller

Copy the shape of `stadia_controller` or `ps4_controller` — they are the clearest templates.

1. **Create module**
   - `src/usb_controllers/<name>_controller.h`
   - `src/usb_controllers/<name>_controller.c`
2. **Implement the contract**
   - `bool <name>_is_controller(uint16_t vid, uint16_t pid);`
   - `void <name>_mount_cb(uint8_t dev_addr);`
   - `void <name>_unmount_cb(uint8_t dev_addr);`
   - `void <name>_process_report(uint8_t dev_addr, const uint8_t *report, uint16_t len);`
3. **Normalize and submit**
   - Directions: `dir_bits` with bit0=up, bit1=down, bit2=left, bit3=right
   - Buttons: map to `port2_gamepad_submit(dir_bits, face_a, face_b, face_x, face_y, l, r, start)`
   - Prefer `port2_gamepad_submit` over raw GPIO — it handles classic vs CD32 Port 2
4. **OLED name**
   - Mount: `usb_map_register_gamepad(dev_addr, "ShortName");`
   - Unmount: `usb_map_unregister_gamepad(dev_addr);` + `port2_gamepad_clear();`
5. **Wire into the build**
   - Add `.c` to `src/CMakeLists.txt` under the existing `usb_controllers/` list
6. **Wire into the dispatcher** (`src/usb_hid.c`)
   - `#include "usb_controllers/<name>_controller.h"`
   - In `tuh_hid_mount_cb` / `tuh_hid_umount_cb` / `tuh_hid_report_received_cb` (and the desktop-gamepad branch in `process_report` if needed): detect VID/PID → call module
7. **Build** and flash
8. **Hardware validation** (below)

### Checklist: hardware validation (USB gamepad)

On a live Amiga (or Amiga-compatible port tester):

- [ ] Device appears on OLED **Devices** / **Map Devices** with the expected short name
- [ ] Port 2 directions: up/down/left/right + diagonals
- [ ] Fire / B2 / B3 (classic 3-button map)
- [ ] Unplug → stick recenters / buttons release (`port2_gamepad_clear` path)
- [ ] Toggle Port 2 **CD32** (Shift+Left Amiga+C or OLED Map Devices) → shoulders/start/extra face buttons behave
- [ ] Keyboard + mouse still work while pad is connected (no Core 1 stall / frozen mouse)
- [ ] Serial: mount/unmount messages; with `CONTROLLER_DEBUG=1`, confirm report length/layout once

Optional second-pad / Port 1 checks if the pad is meant to support multi-device:

- [ ] Second gamepad maps correctly when Port 1 is Joy/CD32
- [ ] Llamatron mode (if BT/USB path supports it) left stick → Port 2, right stick → Port 1

### Checklist: Bluetooth change safety

BT work often interacts with Core 1 mouse timing and flash bonding:

- [ ] After pairing / reconnect, mouse still moves (watch for Core 1 pause orphans)
- [ ] Pairing on/off from OLED Settings works; Clear BT pair then re-pair if bonds go stale
- [ ] Prefer `busy_wait_*` inside BT callbacks (not `sleep_ms` / `__wfe`) — see `bluepad32_platform.c`
- [ ] Config saves during BT pause are deferred (`port_config_flush_pending`); USB role toggle must use `port_config_save_immediate`

### Checklist: USB device mode (Amiga → PC / Host Mode UI)

- [ ] OLED Settings → switch to Host Mode UI path (persists + reboots)
- [ ] Real Amiga keyboard types into the PC
- [ ] Port 1 mouse moves cursor; buttons map L/R/M
- [ ] Caps Lock toggle aligns (synthetic pulse path)
- [ ] Port 2 Atari stick → HID gamepad (Axis 0/1 or hat + B0 fire); re-define on MiSTer after firmware changes
- [ ] Switch back to Device Mode (Amiga adapter) and verify host path still works
- [ ] Note: macOS **System Settings → Game Controllers** often ignores generic HID pads; use browser Gamepad API / `ioreg` instead

### Common pitfalls (agents especially)

1. **Skipping `src/CMakeLists.txt`** — new `.c` files are not auto-globbed.
2. **Only wiring mount, not report** — pad shows on OLED but never moves the stick.
3. **Driving joystick GPIOs directly** instead of `port*_gamepad_submit` — breaks CD32 mode.
4. **Touching Core 1 mouse loop casually** — timing regressions show up as lag, reverse, or stalls after BT pairing.
5. **Deferring USB-mode flash writes then rebooting** — role appears “stuck”; use immediate save + verify.
6. **Including Bluepad32 `uni.h` from `usb_hid.c`** — HID type conflicts with TinyUSB; keep BT types isolated behind `bluepad32_platform` accessors and local mirror structs.
7. **Version bump without docs** — update `config.h` + `README.md` + `RELEASE_NOTES.md` together.

### Suggested first tasks for a new agent session

1. Read this file + `README.md` + current `RELEASE_NOTES.md` top entry.
2. Confirm board revision and USB role from serial boot banner.
3. For a bug: identify host vs device mode, then USB vs BT source, then which Amiga output (kbd / mouse Core1 / port1 / port2 / CD32).
4. Make the smallest change in the layer that owns the bug; reuse existing submit/normalize APIs.
5. Build, flash, run the matching validation checklist above.

