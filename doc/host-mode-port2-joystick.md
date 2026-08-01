# Host Mode Port 2 controllers → USB (PC / MiSTer)

**Status:** Phase 1 (Atari 2-button) **shipped in v4.2.0** — validated on Mac and MiSTer  
**Date:** August 2026  
**Goal:** While the adapter is in **Host Mode** (Amiga/Atari I/O → USB), read DB-9 controllers on **Port 2** and present them as a standard USB HID gamepad for PC and [MiSTer FPGA](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki/Input-devices).

**Scope:**

1. **Atari-style digital joysticks** — **done** (dirs + fire + button 2)
2. **Sega Mega Drive / Genesis** — **on hold** (needs physical remapper; research in §3.2)

Related: [`future_work.md`](./future_work.md), [`architecture.md`](./architecture.md), [`archive/amiga-usb-device-mode.md`](./archive/amiga-usb-device-mode.md).

---

## 1) What “Host Mode” means

UI name **Host Mode** = firmware `USB_MODE_DEVICE` (adapter is a USB **device** to the PC).

| Implemented | Deferred |
|-------------|----------|
| Amiga keyboard → USB HID keyboard | Mega Drive 3-/6-btn (remapper + Select protocol) |
| Port 1 quadrature mouse → USB HID mouse | Port 1 as digital joystick → USB |
| **Port 2 Atari 2-btn stick → USB HID gamepad** | CD32 pad **read** |
| HID IF0: report IDs 1=kbd, 2=mouse; **HID IF1: gamepad** | |

Entry points:

- `src/main.c` — device-mode branch: `keyboard_host_in_*`, `mouse_host_in_*`, `joystick_port2_host_in_*`, `usb_hid_device_task()`
- `src/usb_hid_device.c` — TinyUSB descriptors + report drain (kbd + mouse + gamepad)
- `src/platform/amiga/joystick_port2_host_in.c` — Atari Port 2 input (Host Mode)

---

## 2) Target user scenarios

1. **PC / Mac / Linux** — Competition Pro / Atari CX40 / similar into Port 2; OS sees a HID gamepad.
2. **MiSTer FPGA** — same USB HID gamepad; assign as P1/P2 in the Menu core OSD.
3. MiSTer accepts any standard USB HID gamepad; no XInput required.

**macOS note:** System Settings → Game Controllers usually ignores generic HID pads. Use [hardwaretester.com/gamepad](https://hardwaretester.com/gamepad) or `ioreg -p IOUSB -w0 | grep -i Amiga`. After firmware mapping changes, **re-define** the joystick in MiSTer’s Menu.

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

1. Configure direction/button lines as **GPIO_IN** with **pull-up**.
2. Pressed = `gpio_get(pin) == 0` (active low).
3. Poll in the Host Mode main loop (~8 ms).
4. Straight DB-9 cable into Port 2 — no remapper for Atari/Amiga sticks.

### 3.2 Sega Mega Drive / Genesis (ON HOLD)

**Deferred.** MD pads need a **physical pin 5↔7 remapper** because board pin 7 is hardwired +5V while MD Select must be host-driven. Full protocol notes remain below for a later phase.

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

3-button / 6-button protocols: Select HIGH/LOW samples; 6-btn needs extra fast pulses for X/Y/Z/Mode. References in §10.

</details>

---

## 4) USB HID presentation (as shipped)

### Two HID interfaces (required for macOS / Chrome)

A single composite report-ID device (kbd+mouse+gamepad on one interface) enumerates on Mac, but the **Gamepad API ignores** the gamepad collection when Keyboard is the primary usage. Shipped layout:

```text
HID interface 0 — Keyboard (report ID 1) + Mouse (report ID 2)
HID interface 1 — Gamepad only (no report ID)   ← Port 2 stick
```

`CFG_TUD_HID = 2` in `tusb_config.h`. Product string: **Amiga HID Bridge** (VID `0x2E8A`, PID `0xAB1A`).

TinyUSB gamepad layout: 6× int8 axes, 1× hat, 32 button bits via `tud_hid_n_gamepad_report(1, ...)`.

### Mapping — Atari (Phase 1, frozen)

| Physical | HID field |
|----------|-----------|
| Up / Down / Left / Right | Axis X/Y (±127) + `hat` |
| Fire (pin 6) | Button bit 0 (`GAMEPAD_BUTTON_A` / B0) |
| Button 2 (pin 9) | Button bit 1 (`GAMEPAD_BUTTON_B`) |
| Button 3 (pin 5) | unused (2-btn profile) |
| Unused axes | 0 (centered) |

**Do not** mirror dirs onto buttons 12–15: bit 12 is **Mode**, which MiSTer often uses as OSD/menu — holding a direction then looks like Menu+D-pad (autofire period / OSD).

On MiSTer / many UIs, B0 is labelled **Button 1** (1-based).

### Mapping suggestion — Mega Drive (when resumed)

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
| D-pad | Hat (+ X/Y) | `hat` / axes |

### Poll interval

HID endpoint interval 10 ms; Port 2 poll ~8 ms. Periodic reports (not change-only) keep browsers listing the pad.

---

## 5) Software architecture (implemented)

```text
Host Mode main loop
  tud_task()
  usb_hid_device_task()
  keyboard_host_in_task()
  mouse_host_in_task()
  joystick_port2_host_in_task()
        |
        | Atari: inputs + pull-ups; build x/y + hat + buttons
        v
  usb_hid_device_send_gamepad(x, y, hat, buttons)
        |
        v
  usb_hid_device.c  IF1 → tud_hid_n_gamepad_report(...)
```

| Piece | Role |
|-------|------|
| `joystick_port2_host_in.c/.h` | Atari digital read (**done**) |
| `megadrive_port2_host_in` *(future)* | Select protocol; 3-/6-btn decode |
| `usb_hid_device.c/.h` | IF0 kbd/mouse + IF1 gamepad (**done**) |
| `main.c` | Host Mode init + task (**done**) |

Do **not** reuse `joystick_port2.c` / `cd32_pad.c` Amiga **output** drivers in Host Mode.

### Conflict rules in Host Mode

- Atari mode: Port 2 direction/button GPIOs stay **inputs**.
- MD mode (future): `QM2_AMIGA_B3` becomes **Select output** via remapper; never treat board pin 7 as Select.
- Do not run Amiga-facing Port 2 / CD32 output drivers.
- Port 1 remains mouse-in unless a later dual-pad feature is added.

---

## 6) Phased delivery

### Phase 1 — Atari 2-button — **DONE** (v4.2.0)

- `joystick_port2_host_in.c` reads dirs + fire + button 2
- USB HID gamepad on **its own interface** (IF1); kbd/mouse on IF0
- Straight DB-9 cable; validated on Mac (browser Gamepad API) and MiSTer (Minimig)

### Phase 1b / 2 — Mega Drive — **ON HOLD**

- Physical pin 5↔7 remapper + Select protocol (3-btn then 6-btn)

### Phase 3 — Optional CD32 pad **input**

- Different protocol (Latch/Clock/Data as Amiga). Separate from MD Select.

### Phase 4 — Optional Port 1 Host Mode joystick / second pad

- Second gamepad HID interface for two DB-9 devices.

---

## 7) MiSTer-specific notes

From MiSTer’s [Input devices](https://github.com/MiSTer-devel/Wiki_MiSTer/wiki/Input-devices) wiki:

- Any USB HID device is recognized.
- Press a button after a core starts to assign P1, then P2, etc.
- Define the controller once in the **Main** menu joystick mapping.
- Plain HID Gamepad is fine (no XInput required).
- After firmware changes that alter button bits, **re-define all joysticks**.

**Atari stick test:** Menu → map axes/hat + Button 1 (fire) → arcade/Amiga core.

**MD pad test (future):** Menu → map D-pad + A/B/C/Start (+ X/Y/Z/Mode for 6-btn) → Genesis core.

---

## 8) Lessons learned / risks for MD

| Item | Note |
|------|------|
| Composite kbd+mouse+gamepad on one IF | macOS/Chrome Gamepad API blind — use separate IF |
| Buttons 12–15 as “d-pad” | Bit 12 = Mode → MiSTer OSD/autofire clash |
| Rev 5 level shifter vs Select output | Bench-test; Rev 6 preferred for MD |
| Straight MD cable | Will not work; needs remapper |
| `CFG_TUD_HID_EP_BUFSIZE` | Must fit gamepad report (+ report ID on IF0) |

Open for MD phase: remapper + Select-on-B3 with real 3-/6-btn pads; OLED Atari/MD profile; frozen MD button map.

---

## 9) Implementation checklist (Atari — done)

1. ~~Hardware smoke test: Atari stick on Port 2 as inputs~~
2. ~~Raise `CFG_TUD_HID_EP_BUFSIZE` if needed~~
3. ~~Separate HID IF for gamepad~~
4. ~~`joystick_port2_host_in` → gamepad report~~
5. ~~Validate Mac + MiSTer; document Mode/button pitfalls~~

Remaining: MD remapper + Select protocol when resumed.

---

## 10) References

- Existing Host Mode: `src/usb_hid_device.c`, `mouse_host_in.c`, `keyboard_host_in.c`, `joystick_port2_host_in.c`
- Port 2 Amiga output (do not reuse as Host Mode driver): `joystick_port2.c`
- TinyUSB: `TUD_HID_REPORT_DESC_GAMEPAD` / `tud_hid_n_gamepad_report`
- Architecture: [`architecture.md`](./architecture.md) § device-mode diagram
- MD protocol: [msarnoff gen2usb](http://www.msarnoff.org/gen2usb/), [Interface Protocol of SEGA MegaDrive 6-Button Controller](https://applause.elfmimi.jp/md6bpad-e.html), [AllPinouts SMS/MD](https://allpinouts.org/pinouts/connectors/videogame/segamaster-system-sms-and-megadrive-joystick/)
