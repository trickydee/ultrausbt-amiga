# Code cleanup and optimization candidates

Advisory review of the original borb/amigahid-pico codebase versus ultrausbt additions. **No changes implemented** — use this as a checklist when hardening release builds or reducing maintenance burden.

**Branch context:** `feature/cd32` (firmware v2.2.10 at time of review).

---

## Executive summary

The **borb core** (keyboard serial bit-bang, `amiga_service()`, TinyUSB callbacks) is still the spine and should stay. A fair amount around it is either **legacy from the original tree**, **debug left on after bring-up**, or **parallel implementations** added during the ultrausbt expansion.

The biggest wins are:

1. **Gate or remove unconditional `printf` / `ahprintf`**
2. **Retire dead debug infrastructure**
3. **Trim stub/dead files**
4. **Consolidate controller → port routing** (maintenance + CD32 correctness, not just size)

`doc/performance_optimizations.md` is **partially outdated** — several items (BT early return, keyboard→display path) are already fixed or commented out.

---

## 1. Modifier / toggle key debug spam (high priority)

### Problem

Holding modifier keys (Shift, Left Amiga / GUI) floods UART on **every keyboard report**, not only when a toggle fires. Example from a BT keyboard session:

```
[TOGGLE-BT] Shift:0 LAmiga:1 J:0 Combo:0 Modifier:0x08 Keys:
[TOGGLE-BT] Shift:0 LAmiga:1 J:0 Combo:0 Modifier:0x08 Keys:
... (repeats while Left Amiga held)
[TOGGLE-BT] Shift:1 LAmiga:0 J:0 Combo:0 Modifier:0x02 Keys:
... (repeats while Shift held)
[TOGGLE-BT] Shift:1 LAmiga:1 J:1 Combo:1 Modifier:0x0a Keys: 0x0d
[CONFIG] Saving: mouse=Amiga port1=1 port2_cd32=1
[TOGGLE-BT] *** Port 1 mode: JOY ***
[TOGGLE-BT] Blocking J key from being sent to Amiga
```

This is noisy during normal use, competes with USB/BT timing on the serial link, and makes real errors harder to spot.

### Source locations

| Tag | File | Lines (approx.) | Behaviour |
|-----|------|-----------------|-----------|
| `[TOGGLE-BT]` modifier trace | `src/usb_hid.c` | ~907–917 | Prints whenever **any** of Shift, LAmiga, J is pressed, or combo is active — **every BT keyboard report** while modifiers held |
| `[TOGGLE-BT] *** Port 1 mode` | `src/usb_hid.c` | ~922 | On successful Port 1 mouse/joy toggle (acceptable as one-shot; keep or gate) |
| `[TOGGLE-BT] Blocking J key` | `src/usb_hid.c` | ~959 | Every report while J blocked during combo |
| `[LLAMATRON-BT]` / `[CD32-BT]` | `src/usb_hid.c` | ~927–968 | Same pattern for L and C chords |
| `[TOGGLE]` (USB path) | `src/usb_hid.c` | ~643–746 | **Identical** modifier spam for USB keyboards (`[TOGGLE]` prefix) |
| `[CONFIG] Saving:` | `src/platform/amiga/mouse_config.c` | ~102 | Unconditional `printf` on every flash save (triggered by each mode toggle) |

All `[TOGGLE*]` / `[LLAMATRON*]` / `[CD32*]` lines use **`ahprintf`**, which is enabled whenever root `CMakeLists.txt` sets `DEBUG_MESSAGES=1` (currently always on).

### Recommendation

- **Remove** the per-report modifier trace blocks (USB ~685–695, BT ~907–917) from release builds entirely.
- **Keep one-shot** messages on actual toggle (`*** Port 1 mode`, CD32 on/off) only behind a debug flag, or drop them if OLED already reflects mode.
- **Remove** “Blocking J/L key” per-report lines; log once on combo edge if still needed for bring-up.
- Gate `[CONFIG] Saving:` behind `DEBUG_MESSAGES` or a dedicated `CONFIG_DEBUG` — flash save on every toggle is expected behaviour, not something users need in serial output.
- Consider a single compile flag, e.g. `KEYBOARD_TOGGLE_DEBUG`, shared by USB and BT paths (mirrors existing `KEYBOARD_HID_DEBUG` pattern in `config.h`).

---

## 2. Safe to remove or simplify (low risk)

### Dead / hollow code from borb era

| Item | Location | Notes |
|------|----------|--------|
| **`keyboard.pio`** | `src/platform/amiga/keyboard.pio` + CMake `pico_generate_pio_header` | Stub only (`amiga_send_pio_init` does nothing). `keyboard_serial_io.c` includes `keyboard.pio.h` but never uses PIO. Remove PIO build + include. |
| **`hid_app_task()`** | `usb_hid.c` (empty stub); `extern` in `main.c` but **never called** | Leftover from original `main` loop; main uses `tuh_task()` only. |
| **`dbgcons_*` counter machinery** | `debug_cons.c` / `debug_cons.h` | Counters increment on plug/unplug but **nothing reads them**; `dbgcons_print_counters()` is empty; `dbgcons_amiga_key` / `dbgcons_amiga_mod` are no-ops. Only `dbgcons_init()` VT100 clear + `dbgcons_plug/unplug` from `usb_hid.c` still run. **Candidate to delete entire module** and drop plug/unplug calls. |
| **`cd32_service()`** | `cd32_pad.c` | Empty function; still called every main-loop iteration. Remove call + symbol, or repurpose if deferred JOYMODE work returns. |
| **`*_connected_count()`** | PS3/4/5, Xbox, Stadia, Switch, PSC, HORI headers + `.c` | Exported but **never called** anywhere in the tree. |
| **`joystick_port2_get_*()`** | `joystick_port2.c` | Marked “for debugging”; no callers found. |
| **`main.c` `extern hid_app_task`** | Dead reference | Remove with `hid_app_task`. |

### Stale documentation (misleading, not runtime cost)

- `doc/oled_display_analysis.md` — still describes **µgui**; display is **ssd1306** now.
- `doc/performance_optimizations.md` — §1 (keystroke→display) largely fixed; §2 (BT every loop) **already implemented** via `process_bluepad32_devices()`.
- `doc/debug_logging_removed.md` — title says “removed” but body documents **added** logging; confusing.
- Root README still has old “pin mappings not in source” note while Rev 5 is in `config.h`.

---

## 3. Debug / logging — high value cleanup

### Two logging systems, inconsistent gating

| Path | Gating | Issue |
|------|--------|--------|
| **`ahprintf` / `DEBUG_MESSAGES`** | Root `CMakeLists.txt`: **`DEBUG_MESSAGES=1` always on** | All `ahprintf` in `usb_hid.c` (CD32 toggles, modifier spam, etc.) goes to UART in “release” builds. |
| **`printf` in vendor drivers** | **Always on** — not behind `DEBUG_MESSAGES` | PS3/PS4 especially noisy. |
| **`logi` in Bluepad32 platform** | Bluepad32 compile config | Very chatty on connect/discover; `[DIAG]` paths still present; contributes to UART corruption under CD32+BT (`doc/future_work.md`). |
| **`printf` in `main.c`** | Always on | Startup banner + `[GPIO]` — fine for dev; optional compile flag for production. |
| **`printf` in `mouse_config.c`, `port_mode.c`, `cd32_pad.c`** | Always on | Config save/load and mode changes on every toggle. |
| **`KEYBOARD_HID_DEBUG`** | **Off** in `config.h` ✓ | Good pattern — extend to toggle/modifier debug and vendor drivers. |

### Worst offenders (gate or remove)

1. **`ps4_controller.c`** — mount banner (~15 lines), first-report hex dump, **every 100th report** `printf` in the hot path.
2. **`ps3_controller.c`** — similar mount banner + init `printf` spam.
3. **`bluepad32_platform.c`** — discovery/ready/disconnect `logi` on every device; `sleep_ms` in callbacks (separate from logging, but pairs with debug-era code).
4. **`display_show_controller_detected()`** — **`sleep_ms(3000)`** blocks Core 0 on every USB controller mount (all vendor drivers call it). Bad for BT pairing timing; consider non-blocking splash or skip when BT busy.

### Suggested logging policy

- Production wireless: `DEBUG_MESSAGES=0`, vendor `printf` behind `ENABLE_CONTROLLER_DEBUG` or reuse `ahprintf`.
- Serialize UART: one path (`ahprintf` or ring buffer drained from main loop) — aligns with `doc/future_work.md` notes.
- Keep **`KEYBOARD_HID_DEBUG`** as the documented pattern (`doc/device_troubleshooting.md`).

---

## 4. Performance improvements (medium effort, real gain)

### Already done (update docs, don’t re-fix)

- **`process_bluepad32_devices()`** — counts first, processes only connected KB/mouse/gamepad.
- **Keyboard → OLED** — `dbgcons_amiga_key` removed from hot path.
- **GPIO watchdog** — 5 s interval, skipped during CD32.

### Still worth considering

| Area | What | Why |
|------|------|-----|
| **Main loop** | No fixed tick; `tuh_task` + `amiga_service` + BT + display run flat out | Usually fine on RP2350@200 MHz; if chasing USB/BT timing, a paced loop may help pairing — test with KB+mouse+gamepad before/after. |
| **BT count functions** | `bluepad32_get_*_count()` scan slots every loop | Cache counts in platform; bump only on connect/disconnect. |
| **Display refresh** | `display_set_bt_counts` / USB counts redraw full Devices/Map screens | Only refresh when on those screens **and** counts changed; avoid `display_show_splash()` on every BT count tick when not on splash. |
| **Core 1 mouse loop** | `sleep_us(50)` every iteration in `quad_mouse.c` | Reasonable yield; when Port 1 CD32 pauses Core 1, pause uses `busy_wait_us(5000)` not `__wfe()` — power/timing only. |
| **Controller detect overlay** | 3 s blocking `sleep_ms` | Blocks Core 0 at plug time — affects multi-device and BT. |
| **Switch delayed init** | `sleep_ms(1)` loops in `switch_controller.c` | Only at init; low priority. |
| **`usb_hid.c` size** | ~1270 lines, duplicate CD32 chord (USB keyboard + BT keyboard + similar Llamatron logic) | Refactor reduces flash and maintenance; modest runtime gain from less duplicate work per tick. |

### Architectural perf/maintainability

- **PS4, Xbox, Stadia** → `port2_gamepad_submit()` (CD32-aware).
- **PS3, PS5, Switch, PSC, HORI** → still call `amiga_joystick_port2_*` directly.

CD32 still works for those via **delegation in `joystick_port2.c`**, but two mapping pipelines remain. Consolidating on `port2_gamepad_submit()` would shrink code and guarantee one CD32 path (`doc/future_work.md` Phase 2).

---

## 5. Original borb code — keep vs replace

### Keep (still the product core)

- `keyboard_serial_io.c` / `keyboard.h` — Amiga key protocol and map.
- `amiga_service()` + timer-driven serial in keyboard path.
- TinyUSB mount/report callbacks in `usb_hid.c` (grew huge, but role is same).
- EPL-2.0 file headers on modified originals.

### Effectively replaced (borb intent, ultrausbt implementation)

| borb idea | Today |
|-----------|--------|
| PIO keyboard sender | Bit-bang GPIO in `keyboard_serial_io.c` |
| µgui / `disp_write` debug display | SSD1306 `display.c` |
| `util.c` in `platform/common` | `gpio_util.c` |
| Single HID keyboard | Full HID host + vendor drivers + BT |
| No gamepad | Entire `usb_controllers/` tree |

---

## 6. Larger removals / consolidation (higher risk — plan + test)

| Topic | Advice |
|-------|--------|
| **Rev 2/4 GPIO in `config.h`** | If only Rev 5 hardware is shipped, `#if HIDPICO_REVISION` branches could shrink flash/complexity — only after confirming no builds for older boards. |
| **`debug_cons` module** | Remove entirely once plug/unplug tracking isn’t needed for OLED (device counts come from `usb_hid` / Bluepad32 now). |
| **Duplicate BT pairing fixes** | `bluepad32_platform.c` still has pre–Atari v22.1.0 patterns (`sleep_ms`, bool pause, double-pause). Porting Atari handoff is **correctness**, not cleanup — but removes fragile debug timing. |
| **`raw_report[64]` on PS3** | Debug-only storage; drop if not used. |
| **Llamatron + CD32 + mouse + joy** | `port_mode.c` is necessary complexity; don’t remove — document mutual exclusion (already there). |

---

## 7. What not to remove without hardware proof

- **GPIO watchdog** — cheap insurance against stuck outputs.
- **`amiga_gpio_reset_all_to_input()` at boot** — 5V back-feed protection.
- **Memory barriers in joystick/mouse paths** — fixed real Port 1 bugs.
- **CD32 ISR minimalism** — recent timing fix; don’t “optimize” back to heavy ISRs.
- **Flash persistence** (`mouse_config` / `port_config`) — user-facing; audit sector vs BTstack TLV before moving sectors only.

---

## 8. Suggested priority order

1. **Quick wins:** Remove `hid_app_task`, empty `cd32_service` call, `*_connected_count()`, `keyboard.pio` build, trim dead `dbgcons` or whole module.
2. **Modifier/toggle spam:** Remove per-report `[TOGGLE]` / `[TOGGLE-BT]` traces; gate one-shot toggle + `[CONFIG]` lines (see §1).
3. **Release hygiene:** `DEBUG_MESSAGES=0` default; wrap PS3/PS4 `printf` and mount banners; reduce or gate `logi` in `bluepad32_platform.c`.
4. **Runtime UX:** Make controller-detect overlay **non-blocking** (or disable in release).
5. **Cache BT device counts** on connect/disconnect.
6. **Refactor:** Route all USB vendor pads through `port2_gamepad_submit()` / `port1_gamepad_submit()`.
7. **Docs:** Refresh `performance_optimizations.md`, archive stale µgui doc, align README with Rev 5 reality.
8. **Deferred:** Atari v22.1.0 BT pairing port; BT slot compaction on disconnect.

---

## Related documents

- `doc/future_work.md` — BT pairing alignment, CD32, Map Devices
- `doc/performance_optimizations.md` — partially stale; cross-check against this list
- `doc/device_troubleshooting.md` — `KEYBOARD_HID_DEBUG` pattern
- `doc/BT_PAIRING_HANDOFF.md` — canonical Atari v22.1.0 reference for BT hardening
