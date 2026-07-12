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

1. **CD32 shift-register timing** — Occasional ghost presses on adjacent buttons (e.g. B also triggers A, Y also triggers G; jump may fire rainbow). **v2.2.10:** advance shift on CLOCK **fall** so DATA is stable before Amiga samples on rise (`cd32_pad.c`). If still flaky:
   - Minimal dumb-mode GPIO updates immediately on JOYMODE rise in ISR
   - Atomic `buttons` → `buttons_shadow` copy at latch

2. **BT gamepad slot routing on disconnect** — Slots are assigned by **connection order** (`bt_gamepads[0]` → Port 2, `[1]` → Port 1), not by port intent. Powering off the first-paired pad leaves the survivor in slot 1 while routing only reads slot 0 for Port 2 and requires `count > 1` for Port 1 → **no pad input** until re-pair. Reconnect may land in slot 0 (Port 2) regardless of desired port. Fixes to explore:
   - Compact slots when a pad disconnects (promote slot 1 → 0)
   - Stable assignment by Bluetooth address or user “bind to port” UI

3. **UART / serial log corruption** — Debug output can become garbled mid-line (e.g. `Mouse: Unsupported page: 0xff43…` interleaved with random bytes). Reboot usually clears it; worse when CD32 IRQ load is high or multiple BT devices are active. Likely **non-re-entrant concurrent `printf`/`logi()`** from different contexts (main loop, Bluepad32 BT callbacks, USB stack) writing stdout/UART without serialization. The `0xff43` lines come from Bluepad32 `uni_hid_parser_mouse.c` logging vendor HID pages on every report — high spam rate makes collisions more visible. Fixes to explore:
   - **Serialized logging** — ring buffer drained only from main loop; no direct `printf` from callbacks/ISRs
   - **Mutex or critical section** around all UART output (main + `logi`)
   - **Suppress or rate-limit** Bluepad32 mouse “Unsupported page” logs (especially vendor page `0xff43`)
   - **Compile-time log levels** — production build with BT/file logging off or `CONFIG_BLUEPAD32_*` verbosity reduced
   - Audit: ensure Core 1 never logs; keep `ahprintf` gated (already behind `DEBUG_MESSAGES`)

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

**Status:** Partially done — Amiga **v2.2.18** fixed Stadia→mouse `consumed=0` (Core 1 loop-counter period). Remaining: land bisect fixes on `main`, flash-layout audit, callback/`BT_GAMEPAD_*` polish.

**Background:** Atari adapter fixed intermittent BLE gamepad pairing hangs (Stadia, Xbox Wireless) in **v22.1.0**. Amiga shares the same stack (Pico 2 W, Bluepad32, dual-core, Core 1 from XIP). Full technical context: **[`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md)**. Family best-practices handout (Atari/Apple/future): **[`doc/BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md)**. Stadia/mouse consume lesson: **[`doc/stadia-controller-verification.md`](./stadia-controller-verification.md)**.

### What to do (in order)

1. **Read** [`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md) — especially §9 (`absolute_time` gate) and *Amiga project status*.
2. **Land** v2.2.18 Core 1 consume fix + pause/watchdog work onto `main` if still on a bisect/feature branch.
3. **Audit flash layout** — `src/platform/amiga/mouse_config.c` vs BTstack TLV (`PICO_FLASH_BANK_TOTAL_SIZE`). Compare with Atari `NVSettings.cpp`.
4. **Polish** `src/bluepad32_platform.c` toward Atari: `bt_callback_busy_wait_ms()`, `BT_GAMEPAD_*_MS` settle/ready delays.
5. **Hardware test matrix** (Pico 2 W, Rev 5):
   - BT keyboard + BT mouse connected → pair Stadia or Xbox → Amiga keyboard, mouse, joysticks still work
   - DIAG: `consumed` tracks `motion_feeds`; `quad_gpio` rises
   - Reboot → bonded devices reconnect
   - Clear pairing keys (splash Left+Right 5 s) → fresh pair
6. **Optional:** CYW43 clock trial — Amiga is 200 MHz; Atari BT uses 225 MHz. Only after steps 1–5 pass.
7. **Siblings:** Port the *knowledge* (do not gate Core 1 host-output on `absolute_time` across BT flash) to Atari/Apple docs if needed — Atari Core 1 already uses loop counters for heartbeat; Apple ADB already has pause/refcount (audit any absolute_time-gated output).

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
| `src/platform/amiga/quad_mouse.c` | Keep v2.2.18 loop-counter consume; pause API polish |
| `src/bluepad32_platform.c` | Callback timing toward Atari |
| `src/config.h` | BT_GAMEPAD_* delay constants where missing |
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
| Port 2 fire on GPIO 26 | Not 5V-tolerant on RP2350; **Rev 6** moves fire/B2/B3 to GPIO 7/0/1 — see `doc/gpio_rev6_adc_avoidance.md` |
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
