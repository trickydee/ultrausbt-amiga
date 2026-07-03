# Future work & regression notes

**Last updated:** June 2026  
**Purpose:** Deferred experiments, roadmap, and lessons learned. For day-to-day task tracking see [`doc/todo.md`](./todo.md).

**Active branch:** `feature/cd32` — CD32 gamepad support (see § CD32 below).

---

## P1 — CD32 controller support (`feature/cd32`)

**Status:** v2.2.2 — dual Port 1 + Port 2 CD32; **known instability** (ghost adjacent buttons, BT slot routing on disconnect). See § CD32 known issues below.

**Goal:** Map USB/BT gamepads to the Amiga **CD32 serial pad protocol** (7 buttons + D-pad), not only standard 3-button joystick.

**Build spec (authoritative):** [`doc/CD32_BUILD_SPEC.md`](./CD32_BUILD_SPEC.md)

**Prior draft (background only):** [`doc/cd32_pad_implementation_plan.md`](./cd32_pad_implementation_plan.md) — GPIO mapping there is **superseded** by the build spec (Clock/Latch are direction pins 2–4, not GPIO 2/3).

### Known issues / next fixes (v2.2.2)

1. **CD32 shift-register timing** — Occasional ghost presses on adjacent buttons (e.g. B also triggers A, Y also triggers G). Likely DATA not stable before CLOCK sample, JOYMODE/dumb-mode window (`cd32_service()` deferral), or dual-port IRQ latency. Fixes to explore:
   - Present next bit on CLOCK **falling** edge (or earlier setup before rise)
   - Minimal dumb-mode GPIO updates immediately on JOYMODE rise in ISR
   - Atomic `buttons` → `buttons_shadow` copy at latch

2. **BT gamepad slot routing on disconnect** — Slots are assigned by **connection order** (`bt_gamepads[0]` → Port 2, `[1]` → Port 1), not by port intent. Powering off the first-paired pad leaves the survivor in slot 1 while routing only reads slot 0 for Port 2 and requires `count > 1` for Port 1 → **no pad input** until re-pair. Reconnect may land in slot 0 (Port 2) regardless of desired port. Fixes to explore:
   - Compact slots when a pad disconnects (promote slot 1 → 0)
   - Stable assignment by Bluetooth address or user “bind to port” UI

### Implementation order

1. **Phase 0** — Verify DB-9 ↔ GPIO 19–22 (Port 2) against KTRL_CD32; pick test games.
2. **Phase 1** — New `src/platform/amiga/cd32_pad.c` + ISRs on Port 2; wire `usb_hid.c` gamepad path.
3. **Phase 2** — All USB vendor drivers + BT + OLED toggle + flash persist.
4. **Phase 3** — Hardware test matrix in build spec §8.
5. **Phase 4 (deferred)** — Port 1 CD32 (needs Core 1 mouse pause).

### Key decisions (already made in spec)

| Topic | Decision |
|-------|----------|
| First port | **Port 2** (USB/BT gamepads already route there; no mouse conflict) |
| Mode select | **User toggle** (OLED / button), not auto-detect |
| Default mapping | Xbox-style: B/A/Y/X + triggers + Start → CD32 colours |

### Files to create / modify

| File | Action |
|------|--------|
| `src/platform/amiga/cd32_pad.c` | **Create** |
| `src/platform/amiga/cd32_pad.h` | **Create** |
| `src/usb_hid.c` | CD32 branch in gamepad handlers |
| `src/usb_controllers/*.c` | Route through shared CD32 mapper |
| `src/platform/amiga/joystick_port2.c` | Delegate when CD32 enabled |
| `src/display/display.c` | Mode indicator + toggle |
| `src/config.h` | CD32 defines |

### Do not block on

- Bluetooth pairing v22.1.0 port (orthogonal; do CD32 first on `feature/cd32` if preferred).
- Port 1 CD32 or auto-detect (later phases).

---

## P2 — Bluetooth pairing alignment (Atari v22.1.0)

**Status:** Open — **start here** for the next BT reliability pass.

**Background:** Atari adapter fixed intermittent BLE gamepad pairing hangs (Stadia, Xbox Wireless) in **v22.1.0**. Amiga shares the same stack (Pico 2 W, Bluepad32, dual-core, Core 1 from XIP) but has only **partial** mitigations. Full technical context is in **[`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md)**.

### What to do (in order)

1. **Read** [`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md) — especially *Amiga project status* and *Suggested porting order*.
2. **Audit flash layout** — `src/platform/amiga/mouse_config.c` uses the last 4 KiB sector; confirm it does not overlap BTstack TLV (`PICO_FLASH_BANK_TOTAL_SIZE`). Compare with Atari `NVSettings.cpp`.
3. **Port Core 1 pause/refcount** from Atari `src/main.cpp`:
   - `g_core1_pause_depth`, `core1_pause_for_bt_enumeration()`, `core1_resume_after_bt_enumeration()`
   - `core1_wait_for_pause_active()`
   - `__wfe()` in Core 1 pause branch (`quad_mouse.c`)
4. **Update** `src/bluepad32_platform.c`:
   - Add `bt_callback_busy_wait_ms()`; remove `sleep_ms` from discovery/ready paths
   - Remove double-pause in `on_device_connected` for Xbox/Stadia
   - Discovery: pause → wait-for-pause → 30 ms settle
   - Ready: 100 ms busy-wait → resume (only if `pause_depth > 0`)
   - Disconnect: resume only if `pause_depth > 0`
5. **Add** to `src/config.h`: `BT_GAMEPAD_DISCOVERY_SETTLE_MS` (30), `BT_GAMEPAD_CORE1_RESUME_DELAY_MS` (100).
6. **Hardware test matrix** (Pico 2 W, Rev 5):
   - BT keyboard + BT mouse connected → pair Stadia or Xbox → Amiga keyboard, mouse, joysticks still work
   - Reboot → bonded devices reconnect
   - Clear pairing keys (splash Left+Right 5 s) → fresh pair
7. **Optional:** CYW43 clock trial — Amiga is 200 MHz (`CMakeLists.txt`); Atari BT uses 225 MHz. Only after steps 1–6 pass.

### Reference tree (canonical implementation)

```
../Atari-Keyboard/ultramegausb-atari-st-rpikbd/
  src/main.cpp
  src/bluepad32_platform.c
  include/config.h
  src/NVSettings.cpp
  docs/BT_PAIRING_HANDOFF.md   # generic family doc (upstream)
```

### Amiga files to touch

| File | Change |
|------|--------|
| `src/platform/amiga/quad_mouse.c` | `__wfe()` pause loop; pause/refcount API |
| `src/bluepad32_platform.c` | Callback timing, remove double-pause |
| `src/config.h` | BT_GAMEPAD_* delay constants |
| `src/platform/amiga/mouse_config.c` | Flash sector layout (if overlap found) |

### Do not do before pairing pass is green

- Drop main-loop timing / faster HID polling without retesting KB+mouse+gamepad pair
- Upgrade BTstack past pico-sdk pin without porting `hids_host` + Bluepad32
- Assume `doc/todo.md` “Stadia fix” / “DS5 pairing fix” means v22.1.0 is already ported

---

## P2 — UI (deferred from feature/ui-alignment)

| Item | Notes |
|------|-------|
| Cycle gamepad bindings on Map Devices | Atari `docs/UI_UNIFICATION.md` Phase 2 |
| Portable OLED spec | Local copy: `local/ULTRAMEGAUSB_OLED_UI_SPEC.md` (gitignored) |

---

## P2 — Documentation & cleanup

| Item | Notes |
|------|-------|
| Update `doc/gpio_pin_mapping.md` | Stale vs `src/config.h` (Rev 5) |
| Cleanup review §5–§7 | debug_cons trim, doc consolidation, rev 2/4 removal — user deferred |
| License compliance | See `doc/todo.md` |

---

## P3 — Hardware & enhancements

| Item | Notes |
|------|-------|
| Port 2 fire on GPIO 26 | Not 5V-tolerant on RP2350; consider PCB remap |
| Atari mouse support merge | See `doc/todo.md` — may overlap with existing `quad_mouse` type toggle |
| Submodule / SDK bump | Re-test BT pairing matrix after any pico-sdk or bluepad32 update |

---

## Resolved (keep for reference)

| Item | Version / note |
|------|----------------|
| Map Devices UI + build-all alignment | v2.1.1 |
| Early Stadia / Core 1 pause (partial) | Pre–v22.1.0; superseded by full Atari port above |
| Mouse config flash at firmware overlap | Moved to last sector — **re-audit vs BTstack TLV** in P1 |

---

## Documentation map

| Document | Role |
|----------|------|
| **`doc/future_work.md`** (this file) | Open work, regression lessons, **where to start** |
| **`doc/BT_PAIRING_HANDOFF.md`** | Full BT pairing technical handoff (family + Amiga status) |
| **`doc/todo.md`** | Granular task checklist |
| **`doc/CONTINUE_HERE.md`** | Session resume notes |
| **`doc/submodule-versions.md`** | Submodule pins |
