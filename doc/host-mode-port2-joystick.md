# Research: Host Mode Port 2 controllers → USB (PC / MiSTer)

**Status:** Phase 1 (Atari 2-button) implemented — **v4.2.0** on `feature/host-mode-port2-joystick`  
**Date:** July 2026  
**Goal:** While the adapter is in **Host Mode** (Amiga/Atari I/O → USB), read DB-9 controllers on **Port 2** and present them as a standard USB HID gamepad for PC and [MiSTer FPGA](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki/Input-devices).

**Current scope:**

1. **Atari-style digital joysticks** — **implemented** (dirs + fire + button 2)
2. **Sega Mega Drive / Genesis** — **on hold** (see §3.2; needs physical remapper)

Related: [`future_work.md`](./future_work.md), [`archive/amiga-usb-device-mode.md`](./archive/amiga-usb-device-mode.md), [`architecture.md`](./architecture.md).

---

## 1) What “Host Mode” means today

UI name **Host Mode** = firmware `USB_MODE_DEVICE` (adapter is a USB **device** to the PC).

| Already implemented | Not implemented / deferred |
|---------------------|----------------------------|
| Amiga keyboard → USB HID keyboard | Mega Drive 3-/6-btn (on hold — remapper + Select protocol) |
| Port 1 quadrature mouse → USB HID mouse | Port 1 as digital joystick → USB |
| **Port 2 Atari 2-btn stick → USB HID gamepad** | CD32 pad **read** |
| HID IF0: report IDs 1=kbd, 2=mouse; **HID IF1: gamepad** (separate for macOS) | |

Entry points today:

- `src/main.c` — device-mode branch: `keyboard_host_in_*`, `mouse_host_in_*`, `joystick_port2_host_in_*`, `usb_hid_device_task()`
- `src/usb_hid_device.c` — TinyUSB descriptors + report drain (kbd + mouse + gamepad)
- `src/platform/amiga/joystick_port2_host_in.c` — Atari Port 2 input (Host Mode)

---

## 2) Target user scenarios

1. **PC / Mac / Linux** — Competition Pro / Atari CX40 / similar into Port 2; OS sees a HID gamepad.
2. **MiSTer FPGA** — same USB HID gamepad; assign as P1/P2 in the Menu core OSD. Prefer Atari sticks for arcade/Amiga cores and **Mega Drive pads** for Genesis/MD cores (and general digital play).
3. MiSTer accepts any standard USB HID gamepad; no proprietary protocol required once mapped.

---

## 3) Port 2 electrical / pin maps

### 3.1 Amiga / Atari digital joystick (native Port 2 pinout)

| DB-9 pin | Signal | Active | Typical stick |
|----------|--------|--------|---------------|
| 1 | Up | Low to GND | Yes |
| 2 | Down | Low to GND | Yes |
| 3 | Left | Low to GND | Yes |
| 4 | Right | Low to GND | Yes |
| 5 | Button 3 / PotX / CD32 Latch | Low (Amiga 3-btn) | Often unused on 1-btn Atari sticks |
| 6 | Fire 1 | Low to GND | Yes |
| 7 | +5V | Supply | Stick usually passive switches |
| 8 | GND | Common | Yes |
| 9 | Fire 2 / PotY / CD32 Data | Low (Amiga 2-btn) | Some 2-btn sticks |

Firmware Port 2 map (`config.h`):

| Signal | Rev 5 GPIO | Rev 6 GPIO | Define |
|--------|------------|------------|--------|
| Up | 19 | 19 | `QM2_AMIGA_V` |
| Down | 20 | 20 | `QM2_AMIGA_H` |
| Left | 21 | 21 | `QM2_AMIGA_VQ` |
| Right | 22 | 22 | `QM2_AMIGA_HQ` |
| Fire | 26 | 16 | `QM2_AMIGA_B1` |
| Button 2 | 27 | 17 | `QM2_AMIGA_B2` |
| Button 3 | 28 | 18 | `QM2_AMIGA_B3` |

### How reading works (classic digital stick)

Mirror `mouse_host_in.c` button setup:

1. Configure direction/button lines as **GPIO_IN** with **pull-up**.
2. Pressed = `gpio_get(pin) == 0` (active low).
3. Poll in the Host Mode main loop (1–8 ms is fine).
4. Debounce optional (1–2 samples).

**No Amiga required** for a passive stick. Straight DB-9 cable into Port 2 is fine for Atari/Amiga sticks.

### 3.2 Sega Mega Drive / Genesis (ON HOLD)

**Deferred.** Do not implement until Atari Host Mode is validated on hardware. MD pads need a **physical pin 5↔7 remapper** because board pin 7 is hardwired +5V while MD Select must be host-driven. Full protocol notes remain below for a later phase.

<details>
<summary>MD research notes (deferred)</summary>

#### Why a straight cable will not work

Mega Drive DE-9 pinout **differs** from Amiga Port 2 on the critical control pins ([iComp DE-9 table](https://wiki.icomp.de/wiki/DB9-Joystick), Sega docs):

| DB-9 pin | Amiga Port 2 (this board) | Mega Drive pad |
|----------|---------------------------|----------------|
| 1–4 | Up / Down / Left / Right | Up / Down / Left / Right (same idea) |
| 5 | Button 3 / PotX | **+5V** |
| 6 | Fire | **TL** (A when Sel low, B when Sel high) |
| 7 | **+5V (fixed)** | **TH / Select (host must drive)** |
| 8 | GND | GND |
| 9 | Button 2 | **TR** (Start when Sel low, C when Sel high) |

#### Required Host Mode MD adapter cable

| MD pad pin | MD signal | Wire to Amiga Port 2 pin | Board GPIO / rail | Host Mode role |
|------------|-----------|--------------------------|-------------------|----------------|
| 1–4 | dirs | 1–4 | Port 2 dir GPIOs | Input |
| 5 | +5V | **7** | +5V rail | Power to pad |
| 6 | TL (A/B) | 6 | `QM2_AMIGA_B1` | Input |
| 7 | Select | **5** | `QM2_AMIGA_B3` | **Output (Select)** |
| 8 | GND | 8 | GND | Ground |
| 9 | TR (Start/C) | 9 | `QM2_AMIGA_B2` | Input |

3-button / 6-button protocols: see prior research (Select HIGH/LOW samples; 6-btn needs extra fast pulses for X/Y/Z/Mode). References in §10.

</details>

---

## 4) USB HID presentation (PC + MiSTer)

### Recommended approach: extend the existing composite HID

Today (`usb_hid_device.c`):

```text
Report ID 1 → Keyboard
Report ID 2 → Mouse
```

Add:

```text
Report ID 3 → Gamepad  (TUD_HID_REPORT_DESC_GAMEPAD)
```

TinyUSB already provides:

- Descriptor macro: `TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(3))`
- Helper: `tud_hid_n_gamepad_report(...)` / `hid_gamepad_report_t`
- Layout: 6× int8 axes, 1× hat/DPAD, 32 button bits

Same HID report serves Atari sticks and MD pads; only the Port 2 decoder changes. MiSTer / PC / RetroArch all accept Generic Desktop Gamepad + Hat + Buttons.

### Mapping suggestion — Atari (Phase 1)

| Physical | HID field |
|----------|-----------|
| Up / Down / Left / Right | Axis X/Y (±127) + `hat` (not buttons 12–15 — Mode clash on MiSTer) |
| Fire (pin 6) | Button bit 0 (`GAMEPAD_BUTTON_A`) |
| Button 2 (pin 9) | Button bit 1 |
| Button 3 (pin 5) | unused (2-btn profile) |
| Unused axes | 0 (centered) |

### Mapping suggestion — Mega Drive (Phase 1b / 2)

| MD button | HID suggestion | TinyUSB bit (typical) |
|-----------|----------------|------------------------|
| A | South / A | `GAMEPAD_BUTTON_A` |
| B | East / B | `GAMEPAD_BUTTON_B` |
| C | West / X *or* R1 | Freeze one layout and document; MiSTer remaps |
| Start | Start | `GAMEPAD_BUTTON_START` |
| X | North / Y | `GAMEPAD_BUTTON_Y` |
| Y | L1 | `GAMEPAD_BUTTON_TL` |
| Z | R1 | `GAMEPAD_BUTTON_TR` |
| Mode | Select | `GAMEPAD_BUTTON_SELECT` |
| D-pad | Hat (+ X/Y optional) | `hat` |

Stable bit positions matter more than perfect face-button semantics. Publish the frozen map in README when implementing.

Atari Phase 1 reports dirs on X/Y (±127) and hat only — not buttons 12–15 (bit 12 is Mode and collides with MiSTer OSD/autofire).

### Product string / VID-PID

Current Host Mode identity: VID `0x2E8A` (RPi), PID `0xAB1A`. Consider product string “Amiga HID Bridge”. Optional OLED Host Mode label: **Atari** / **MD3** / **MD6**.

### Poll interval

Existing HID endpoint uses 10 ms. Atari: fine. MD 6-btn: finish the Select sequence inside one poll (≪ 1 ms GPIO), then send one HID report.

---

## 5) Software architecture (proposed, not implemented)

```text
Host Mode main loop (existing)
  tud_task()
  usb_hid_device_task()
  keyboard_host_in_task()
  mouse_host_in_task()
  port2_host_in_task()            ← NEW (mode: Atari | MD3 | MD6 | Auto)
        |
        | Atari: inputs + pull-ups only
        | MD:    drive Select (B3 via remapper), sample muxed lines
        |        build hat + button mask
        v
  usb_hid_device_send_gamepad(hat, buttons)
        |
        v
  usb_hid_device.c  REPORT_ID_GAMEPAD → tud_hid_gamepad_report(...)
```

### Suggested new files / touches

| Piece | Role |
|-------|------|
| `src/platform/amiga/joystick_port2_host_in.c/.h` | Atari digital read |
| `src/platform/amiga/megadrive_port2_host_in.c/.h` *(or same module)* | Select protocol; 3-/6-btn decode |
| `src/usb_hid_device.c/.h` | Gamepad report ID + send API |
| `src/main.c` | Host Mode init + task only |
| OLED Settings / Splash | Port 2 Host type: Atari / Mega Drive / Auto |
| Docs | MD remapper cable pinout |

Do **not** reuse `joystick_port2.c` / `cd32_pad.c` Amiga **output** drivers in Host Mode. MD Select drive is a Host Mode–only output on the remapped B3 line.

### Conflict rules in Host Mode

- Atari mode: Port 2 direction/button GPIOs stay **inputs**.
- MD mode: `QM2_AMIGA_B3` becomes **Select output**; other Port 2 lines stay inputs; never treat board pin 7 as Select.
- Do not run Amiga-facing Port 2 / CD32 output drivers.
- Port 1 remains mouse-in unless a later dual-pad feature is added.

---

## 6) Phased delivery

### Phase 1 — Atari 2-button — **DONE** (v4.1.2)

- `joystick_port2_host_in.c` reads dirs + fire + button 2
- USB HID gamepad on **its own interface** (IF1); kbd/mouse on IF0 — required for macOS/Chrome
- Wired in Host Mode `main.c` loop
- Straight DB-9 cable (no remapper)

### Phase 1b / 2 — Mega Drive — **ON HOLD**

- Physical pin 5↔7 remapper + Select protocol (3-btn then 6-btn)
- Resume after Atari Host Mode is validated on PC / MiSTer

### Phase 3 — Optional CD32 pad **input**

- Different protocol (Latch/Clock/Data as Amiga). Separate from MD Select.

### Phase 4 — Optional Port 1 Host Mode joystick / second pad

- Second gamepad report ID or second HID interface for two DB-9 devices.

---

## 7) MiSTer-specific notes

From MiSTer’s [Input devices](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki/Input-devices) wiki:

- Any USB HID device is recognized.
- Press a button after a core starts to assign P1, then P2, etc.
- Define the controller once in the **Main** menu joystick mapping.
- Plain HID Gamepad is fine (no XInput required).

**Atari stick test:** Menu → map hat + Button 1 → arcade/Amiga core.

**MD pad test:** Menu → map D-pad + A/B/C/Start (+ X/Y/Z/Mode for 6-btn) → Genesis core; confirm Select pulses don’t drop buttons at poll rate.

---

## 8) Risks & open questions

| Risk | Mitigation |
|------|------------|
| Rev 5 level shifter vs Select output / passive inputs | Bench-test; Rev 6 preferred for MD |
| User plugs MD pad with **straight** cable | Clear docs + OLED mode labelling; detect power/Select faults if possible |
| 6-btn timing too slow/fast | Follow published µs pulse widths; verify on real 6-btn pads |
| Auto-detect false positives | Manual Atari / MD3 / MD6 OLED override |
| Composite re-enumeration | Keep report IDs 1–2; append ID 3 only |
| `CFG_TUD_HID_EP_BUFSIZE` too small | Raise above gamepad report size |

Open questions before coding:

1. Confirm Rev 5/6 Port 2 **input** with a real Atari stick.
2. Confirm remapper + Select-on-B3 with real MD 3-btn and 6-btn pads.
3. Default Host Mode Port 2 profile: Atari vs Auto vs last-saved.
4. Frozen HID button bit map for MD (esp. C / X / Y / Z).
5. Product string / PID change when gamepad present?

---

## 9) Implementation checklist (when approved)

1. Hardware smoke test: Atari stick on Port 2 as inputs; UART bit dump.
2. Build MD remapper (swap 5↔7); smoke-test Select GPIO and pad power.
3. Extend `usb_hid_device` with `TUD_HID_REPORT_DESC_GAMEPAD`; bump EP buffer if needed.
4. Implement Atari `port2_host_in` → gamepad report.
5. Implement MD 3-btn Select protocol → same gamepad report.
6. Add MD 6-btn sequence + OLED mode select.
7. Validate PC + MiSTer (Atari + MD3 + MD6); update README / device-mode doc / `future_work.md`.

---

## 10) References

- Existing Host Mode: `src/usb_hid_device.c`, `mouse_host_in.c`, `keyboard_host_in.c`
- Port 2 Amiga output (do not reuse as Host Mode driver): `joystick_port2.c`
- CD32 Amiga output / later input: `cd32_pad.c`, `doc/archive/CD32_BUILD_SPEC.md`
- TinyUSB: `TUD_HID_REPORT_DESC_GAMEPAD` / `tud_hid_gamepad_report`
- MiSTer: [Input devices wiki](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki/Input-devices)
- DE-9 cross-system pinouts: [iComp DE-9 Joystick](https://wiki.icomp.de/wiki/DB9-Joystick)
- MD protocol: [msarnoff gen2usb](http://www.msarnoff.org/gen2usb/), [Interface Protocol of SEGA MegaDrive 6-Button Controller](https://applause.elfmimi.jp/md6bpad-e.html), [AllPinouts SMS/MD](https://allpinouts.org/pinouts/connectors/videogame/segamaster-system-sms-and-megadrive-joystick/)
