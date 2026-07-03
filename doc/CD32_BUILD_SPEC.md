# CD32 controller support — build specification

**Branch:** `feature/cd32`  
**Status:** Implemented (v2.2.2) — dual Port 1 + Port 2 CD32; **known instability** (see §9 risks + `doc/future_work.md`)  
**Hardware target:** Rev 5 (`HIDPICO_REVISION == 5`) — Pico 2 W + ultramegausb board  
**Prior research:** [`doc/cd32_pad_implementation_plan.md`](./cd32_pad_implementation_plan.md) (2024 draft — superseded; kept for protocol background)  
**Protocol reference:** [PSCD32 Development Diary, 9 Aug 2019](https://www.mrdictionary.net/PSCD32/diary/2019_08_09.htm) (Mathew Carr) — analysis of Gerd Kautzmann’s CD32 pad schematic; **authoritative DB-9 pin roles** below.

---

## 1. Goal

Enable **Amiga CD32 gamepad protocol** emulation so USB and Bluetooth gamepads expose the **seven face/trigger/start buttons** (plus D-pad) that CD32 software expects — not only the three buttons of a standard Amiga joystick.

**User-visible outcome:**

- Toggle a port into **CD32 mode** when playing CD32-enhanced Amiga titles.
- Map modern gamepad inputs → CD32 **Blue / Red / Yellow / Green / R / L / Start**.
- Keep existing **mouse**, **standard joystick**, and **Llamatron** modes unchanged when CD32 is off.

**Non-goals (v1):**

- Emulating a real CD32 console (audio, CD, etc.).
- Auto-detecting CD32 vs standard joystick from the Amiga side (optional later).
- Per-game mapping profiles or remapping UI.
- CD32 on Rev 2/4 boards (Rev 5 only for v1).

---

## 2. Protocol summary (from PSCD32 diary + pad schematic)

The CD32 pad is **not** “Clock/Latch/Data on direction pins 2–4”. That model (seen in some adapter projects) does **not** match the real Commodore CD32 controller hardware.

### How the real CD32 pad works

The Amiga joyport has **three bidirectional lines** (pins 5, 6, 9 — POT0X, FIRE0, POT0Y) that Paula/Akiko can drive or read. The CD32 pad uses a **74LS165** shift register plus **74LS125** tri-state buffers. Two modes, selected by **JOYMODE** on **pin 5**:

| JOYMODE (pin 5) | Mode | Behaviour |
|-----------------|------|-----------|
| **High** | Dumb 2-button joystick | **Red** grounds pin 6 (Fire1); **Blue** grounds pin 9 (Fire2). Like SMS / 2-button joy. |
| **Low** | 7-button shift mode | On **falling edge** of JOYMODE, latch Blue/Red/Yellow/Green/FF/Rew/Pause into shift register. **Blue** appears on pin 9 (DATA). Each **rising edge** on pin 6 (CLOCK) shifts next bit. |

**Shift order** (on pin 9 DATAOUT, active low when pressed):  
**Blue → Red → Yellow → Green → FF (right shoulder) → Rew (left shoulder) → Pause → 1 → 0 → 0 → …**

**D-pad** pins 1–4 (Up/Down/Left/Right) are **always** simple switches to ground — unchanged in both modes.

**Pin 6 (FIRE0 / CLOCKIN)** is **bidirectional**: output (Red fire) when JOYMODE high; **input** (clock) when JOYMODE low. Firmware must **release/tri-state** pin 6 in shift mode to avoid bus contention with the Amiga.

**Bus contention:** Games can flip JOYMODE at any time. The pad (our firmware) must never fight the Amiga driving the same line — use open-drain / high-Z where appropriate (PSCD32 uses current-limiting resistors on adapter hardware).

### Authoritative DB-9 pin table (PSCD32 / Gerd Kautzmann)

| DB-9 pin | Amiga signal | CD32 pad role | Direction (pad view) |
|----------|--------------|---------------|----------------------|
| 1 | UP | Up | Pad → Amiga (switch) |
| 2 | DOWN | Down | Pad → Amiga (switch) |
| 3 | LEFT | Left | Pad → Amiga (switch) |
| 4 | RIGHT | Right | Pad → Amiga (switch) |
| 5 | POT0X | **JOYMODE / FRAME** | Amiga → Pad |
| 6 | FIRE0 | **FIRE1OUT / CLOCKIN** | Bidirectional |
| 7 | +5V | +5V | Power |
| 8 | GND | GND | Ground |
| 9 | POT0Y | **FIRE2OUT / DATAOUT** | Pad → Amiga (serial data) |

### Rev 5 GPIO mapping (Port 2 — v1 target)

| DB-9 | CD32 role | Port 2 GPIO | Firmware role in CD32 mode |
|------|-----------|-------------|----------------------------|
| 1 | Up | 19 `QM2_AMIGA_V` | Output (switch to GND) |
| 2 | Down | 20 `QM2_AMIGA_H` | Output (switch to GND) |
| 3 | Left | 21 `QM2_AMIGA_VQ` | Output (switch to GND) |
| 4 | Right | 22 `QM2_AMIGA_HQ` | Output (switch to GND) |
| 5 | JOYMODE | 28 `QM2_AMIGA_B3` | **Input** — watch HIGH/LOW + falling edge |
| 6 | CLOCKIN | 26 `QM2_AMIGA_B1` | **Input** (shift mode) or open-drain out (dumb mode / Red) |
| 9 | DATAOUT | 27 `QM2_AMIGA_B2` | **Output** — serial button bits |

Port 1 equivalent: pins 5/6/9 → GPIO **3** (B3), **14** (Fire), **2** (B2); directions → GPIO 10–13.

> **Correction history:** Early drafts assumed Clock/Latch/Data on pins 2–4 (direction lines). The PSCD32 diary shows serial signalling on **pins 5, 6, 9** — which on Rev 5 are exactly our **B3 / Fire / B2** GPIOs, not the quadrature/direction pins.

### Recommended v1 target: **Port 2**

| Factor | Port 1 | Port 2 |
|--------|--------|--------|
| USB gamepad routing today | ❌ All USB pads → Port 2 only | ✅ Primary gamepad port |
| BT gamepad #1 | Port 2 | ✅ |
| Mouse quadrature (Core 1) | ⚠️ Shares GPIO 10–13 | ✅ No conflict |
| Port 1 mode complexity | MOUSE / JOY / LLAMA | N/A |
| CD32 games (typical) | Some titles use Port 1 | Many use Port 2 / either port |

**Decision:** Implement **CD32 on Port 2 first**. Port 1 CD32 is **Phase 2** (requires pausing Core 1 quadrature and extending Port 1 mode machine).

### Mode selection: user toggle (not auto-detect)

Same recommendation as the 2024 plan:

- **User-selectable** CD32 mode via OLED + button (consistent with Port 1 MOUSE/JOY/LLAMA cycle).
- **Persist** preference in flash (extend `mouse_config` or new `port_config` sector — audit vs BTstack TLV per [`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md)).
- **No auto-detect** in v1 (JOYMODE can toggle anytime; optional monitor later).

### ISR design (updated for real protocol)

| Event | Action |
|-------|--------|
| JOYMODE (pin 5) **falling edge** | Latch gamepad state into shift register; present **Blue** on DATA (pin 9); reset bit index |
| CLOCK (pin 6) **rising edge** (JOYMODE low) | Advance shift; drive next bit on DATA |
| JOYMODE **high** | Dumb mode: optionally drive Red on pin 6, Blue on pin 9 as switches; no clock ISR |

No separate “Latch pin” — latch is **JOYMODE falling edge**.

---

## 3. Current firmware baseline

### What exists today

| Capability | Status |
|------------|--------|
| Standard joystick (3 buttons + 4-way) | ✅ Port 1 & 2 |
| USB gamepads → Port 2 | ✅ Generic HID + vendor drivers (`usb_controllers/`, `xbox_controller.c`) |
| BT gamepad #1 → Port 2 | ✅ `process_bluepad32_gamepad()` |
| BT gamepad #2 → Port 1 (joy mode) or mouse buttons | ✅ |
| Llamatron (1× BT pad, 2 ports) | ✅ BT only |
| CD32 serial protocol | ❌ |
| 7-button gamepad mapping | ❌ (only Fire + B2 + B3) |

### Default gamepad → joystick mapping (to replace in CD32 mode)

| Gamepad (Bluepad32) | Standard Port 2 |
|---------------------|-----------------|
| `BUTTON_A` | Fire |
| `BUTTON_B` | Button 2 |
| `BUTTON_X` / `BUTTON_Y` | Button 3 |
| `throttle` / `brake` | *(ignored)* |
| `misc_buttons` (Start/Select) | *(ignored)* |
| Left stick / D-pad | Directions |

Vendor USB drivers (PS4, Xbox, Switch, etc.) follow the same **3-button** ceiling via `amiga_joystick_port2_set_button()`.

---

## 4. CD32 gamepad mapping (target)

Single default layout for v1 (Xbox-style / Bluepad32 names):

| CD32 button | Shift index | Gamepad source (default) |
|-------------|-------------|---------------------------|
| Blue | 0 | `BUTTON_B` |
| Red | 1 | `BUTTON_A` |
| Yellow | 2 | `BUTTON_Y` |
| Green | 3 | `BUTTON_X` |
| R-trigger | 4 | `brake` > threshold **or** `BUTTON_TR` if available |
| L-trigger | 5 | `throttle` > threshold **or** `BUTTON_TL` |
| Start | 6 | `misc_buttons` Start / Pause — PSCD32 calls this **Pause** |
| *(pad)* | 7 | always `1` |
| *(pad)* | 8 | always `0` |

**D-pad / left stick:** Drive GPIO **19–22** (pins 1–4) as parallel switches in both modes — always independent of the shift register.

**Dumb mode (JOYMODE high):** Some titles only read Red/Blue as Fire1/Fire2 on pins 6/9 before entering shift mode. Optionally mirror **Red** → pin 6, **Blue** → pin 9 when JOYMODE is high (verify in Phase 0).

### Input sources when CD32 mode active (Port 2)

| Source | v1 behaviour |
|--------|----------------|
| USB gamepad (first connected) | ✅ Map through CD32 layer |
| BT gamepad #1 | ✅ Map through CD32 layer |
| BT gamepad #2 | Standard Port 1 only (unchanged) |
| Llamatron | **Disabled** while either port CD32 active (mutually exclusive) |

---

## 5. Software architecture

### New module: `cd32_pad.c` / `cd32_pad.h`

Responsibilities:

1. **GPIO mode setup** — directions OUT; pin 5 IN (JOYMODE); pin 6 IN or open-drain OUT; pin 9 OUT (DATA).
2. **GPIO IRQ handlers** — JOYMODE ↓ (latch + first bit); CLOCK ↑ on pin 6 when JOYMODE low (shift). Reference: PSCD32 diary + [KTRL_CD32](https://github.com/MickGyver/KTRL-CD32) (verify KTRL’s wire labels vs DB-9 pin numbers).
3. **Shift register state** — 9 bits, active-low button storage updated from gamepad mapper.
4. **API** for gamepad layer:

```c
void cd32_pad_init(uint8_t port);           // port 2 first
void cd32_pad_enable(bool on);
bool cd32_pad_is_enabled(void);
void cd32_pad_set_buttons(const cd32_buttons_t* b);  // logical CD32 buttons
void cd32_pad_set_dpad_up(bool pressed);    // parallel pin 1
```

```c
typedef struct {
    bool blue, red, yellow, green;
    bool r_trigger, l_trigger, start;
    bool fire;   // parallel pin 6
} cd32_buttons_t;
```

### Integration points

| File | Change |
|------|--------|
| `src/platform/amiga/cd32_pad.c` | **New** — protocol + ISRs |
| `src/platform/amiga/cd32_pad.h` | **New** |
| `src/platform/amiga/joystick_port2.c` | When CD32 off: existing path. When on: delegate directions/fire to `cd32_pad` |
| `src/usb_hid.c` | `cd32_map_from_gamepad()`; branch in `handle_event_gamepad`, `process_bluepad32_gamepad` |
| `src/usb_controllers/*.c` | Each `*_update_amiga_joystick()` → call shared CD32 mapper when enabled |
| `src/config.h` | `CD32_*` GPIO aliases (port 2); `ENABLE_CD32` |
| `src/display/display.c` | Splash/Devices: show `CD32` mode; extend left-button cycle or add Port 2 mode |
| `src/platform/amiga/mouse_config.c` or new `port_config.c` | Persist CD32 enable flag |
| `src/CMakeLists.txt` | Add `cd32_pad.c` |

### Core 0 vs Core 1

| Core | Workload |
|------|----------|
| **Core 0** | CD32 ISRs, gamepad mapping, USB/BT poll |
| **Core 1** | Quadrature mouse on Port 1 GPIO 10–13 only |

**Port 2 CD32 does not require Core 1 changes** for v1.

**Port 1 CD32 (Phase 2):** Must **pause Core 1** (`g_core1_paused` / `amiga_quad_mouse_pause_core1`) while CD32 ISRs own GPIO 10–13.

### Timing

- Latch/Clock pulses: ~10–20 µs (Amiga-driven).
- ISRs must be short: load next bit onto DATA, no `printf`, no flash access.
- Mark ISR paths `__not_in_flash_func` if placed in RAM.
- Consider `gpio_set_irq_callback` with both pins sharing one handler (KTRL pattern).

---

## 6. UI / UX

### Mode indication

Extend **Devices** screen footer (Port 2 line) or splash title when CD32 active:

```
Port2:  CD32
```

### Toggle mechanism (proposal)

**Option A (recommended):** Extend **left button** on Splash/Devices to cycle Port 2:

```
STD → CD32 → STD
```

Keep Port 1 cycle separate: `MOUSE → JOY → LLAMA → MOUSE`.

**Option B (implemented):** Keyboard chord: **Shift + Left Amiga + C** — USB and Bluetooth keyboards (Left GUI = Left Command / Left Windows).

### Map Devices screen

When CD32 active, Map Devices row **J2** could show `CD32` suffix — optional v1.1.

---

## 7. Implementation phases

### Phase 0 — Validation (2–4 h)

- [x] Read PSCD32 diary § “The CD32 stuff” + Gerd Kautzmann schematic mirror.
- [x] Read KTRL_CD32 — **map Arduino pin labels to DB-9 pins 5/6/9**, not direction pins.
- [x] Confirm Rev 5 PCB: pin 5 = GPIO 28, pin 6 = GPIO 26, pin 9 = GPIO 27 on Port 2.
- [ ] Logic analyser on Amiga: capture JOYMODE + CLOCK + DATA during a CD32 game boot (optional).
- [x] Test titles: amiga-test-kit CD32 pad test; *Rainbow Islands* (in-game).

### Phase 1 — Port 2 CD32 core (8–12 h)

- [x] Add `cd32_pad.c/h` with state machine + ISRs (Port 2 GPIOs).
- [x] `cd32_port2_set_enabled()` — directions on GPIO 19–22; JOYMODE GPIO 28 IN; CLOCK GPIO 26 IN/OD; DATA GPIO 27 always OUTPUT.
- [x] `port2_gamepad_submit()` unified mapper from gamepad struct.
- [x] Wire `usb_hid.c` + PS4/Xbox/Stadia + BT `process_bluepad32_gamepad()` through CD32 path.
- [x] **Protocol fixes (v2.1.2):** release CLOCK before JOYMODE latch (avoid bus contention); DATA driven as OUTPUT HIGH/LOW in ISR (KTRL-CD32 style, not INPUT tri-state per bit).

### Phase 2 — Full gamepad routing + UI (4–6 h)

- [x] Route PS4, Xbox, Stadia, generic HID, and BT gamepad #0 through `port2_gamepad_submit()`.
- [x] OLED Devices footer: `Port2: CD32` / `Port2: STD`.
- [x] Toggle: **Shift + Left Amiga + C** (USB + BT keyboards).
- [x] Flash persistence for CD32 flag and Port 1 mode (`port_config` in flash).
- [x] Disable Llamatron when CD32 on (and vice versa).
- [ ] Remaining USB drivers (PS3, PS5, Switch, PSC, HORI) — still on legacy 3-button path when CD32 off; need `port2_gamepad_submit()` when CD32 on.

### Phase 3 — Hardware soak (4–8 h)

- [x] amiga-test-kit CD32 pad detection + per-button mapping (BT Stadia, BT PS5).
- [x] In-game: *Rainbow Islands*.
- [ ] Full matrix in §8.
- [ ] Regression: standard joystick mode, mouse, keyboard, BT pairing unchanged.

### Phase 4 — Port 1 CD32

- [x] `cd32_pad.c` — **independent per-port shift state** (Port 1 + Port 2 can run CD32 simultaneously).
- [x] Core 1 paused when Port 1 CD32 active; mouse buttons blocked on serial GPIOs.
- [x] BT gamepad #2 → `port1_gamepad_submit()` when Port 1 CD32; BT gamepad #1 → Port 2 when Port 2 CD32 (dual-pad setup).
- [x] OLED left button: **MOUSE → JOY → LLAMA → CD32 → MOUSE** (splash + Devices screen).
- [ ] USB gamepad routing to Port 1 CD32 (USB pads still Port 2 only).

---

## 8. Test matrix

| # | Setup | Action | Pass |
|---|--------|--------|------|
| 1 | CD32 off, USB pad | Play standard 1-button game | Fire + directions OK |
| 2 | CD32 on, USB pad | CD32 title | All 7 buttons + D-pad |
| 3 | CD32 on, BT pad #1 | Same | **Pass** (Stadia, PS5) |
| 4 | CD32 on | Toggle off mid-game | Returns to 3-button joy |
| 5 | CD32 on | Reboot | Mode restored from flash — **Pass** (Port 1 mode + Port 2 CD32) |
| 6 | CD32 on + Llamatron attempt | Toggle Llamatron | Llamatron blocked or CD32 disabled with message — **Pass** |
| 7 | CD32 on | USB keyboard + mouse still work | No regression |
| 8 | CD32 on | Pair new BT gamepad | Pairing OK (after BT v22.1.0 port if needed) |
| 9 | Standard joy Port 1 | Mouse + Port 1 unchanged while Port 2 CD32 | No cross-talk |
| 10 | Port 1 CD32 + Port 2 CD32 | Two BT pads (#1 → P2, #2 → P1) | Both shift registers independent; mouse unavailable on P1 |

---

## 9. Risks

| Risk | Mitigation |
|------|------------|
| ISR timing too slow | RAM functions; minimal ISR work; test on real Amiga early |
| Wrong pin assignment (pins 2–4 vs 5/6/9) | Follow PSCD32 / pad schematic; Phase 0 LA capture |
| Bus contention on pin 6 | Open-drain + release when JOYMODE low; never drive CLOCK while Amiga clocks |
| D-pad vs serial | Pins 1–4 always parallel — no conflict |
| Flash persist overlaps BT TLV | Use Atari `NVSettings` sector math before adding flags |
| USB driver duplication | Single `cd32_apply_gamepad(port, buttons, axes)` called from all drivers |
| Games expecting Port 1 CD32 | Document Port 2 default; Phase 4 for Port 1 |
| Shift-register timing (v2.2.2) | Ghost adjacent buttons; fix DATA/CLOCK edge timing — see `doc/future_work.md` |
| BT slot routing on disconnect | First-free-slot assignment; survivor pad unrouted — see `doc/future_work.md` |

---

## 10. Versioning & docs

- Bump `SOFTWARE_VERSION_MINOR` when CD32 ships (e.g. v2.2.0).
- Update README feature list.
- Archive note at top of `cd32_pad_implementation_plan.md` pointing here.

---

## 11. References

| Resource | URL |
|----------|-----|
| **PSCD32 diary (protocol deep-dive)** | https://www.mrdictionary.net/PSCD32/diary/2019_08_09.htm |
| Gerd Kautzmann CD32 pad schematic | http://gerdkautzmann.de/cd32gamepad/cd32gamepad.html |
| KTRL_CD32 (firmware reference) | https://github.com/MickGyver/KTRL-CD32 |
| 9pin2supergun | https://github.com/turmoni/9pin2supergun |
| Amiga Hardware Manual — controller | https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017E.html |
| Prior internal plan | [`doc/cd32_pad_implementation_plan.md`](./cd32_pad_implementation_plan.md) |
| joypad-os button layouts (future) | [`doc/joypad-os-investigation.md`](./joypad-os-investigation.md) |
| Rev 5 GPIO | [`src/config.h`](../src/config.h) |

---

## 12. Quick start for implementers

1. Check out `feature/cd32`.
2. Read this spec + KTRL_CD32 `loop()` / ISR sections.
3. Implement **Phase 0** pin verification on bench.
4. Implement **Phase 1** `cd32_pad.c` for **Port 2 only**.
5. Branch `usb_hid.c` gamepad path through `cd32_pad_set_buttons()`.
6. Run test matrix §8 before merging.

**Start task list:** [`doc/future_work.md`](./future_work.md) § CD32 controller support.
