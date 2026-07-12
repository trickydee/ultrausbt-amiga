# Bluetooth pairing hangs — handoff notes (Pico / Bluepad32 / dual-core)

**Audience:** LLM or developer working on another **ultramegausb** Pico 2 W HID adapter that sees **random Bluetooth pairing hangs** while keyboards/mice work until a gamepad pairs.

**Standalone family best practices (share this with Atari / Apple / future adapters):**  
[`doc/BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md)

**Applies to:** Any sibling project with the same architecture — e.g. **Atari ST IKBD**, **Amiga** keyboard/joystick USB/BT bridge, **Apple ADB** mouse/keyboard adapter — not one host protocol only.

**Reference implementation (fixes shipped):** `ultramegausb-atari-st-rpikbd` — **v22.1.0** (`RELEASE_NOTES.md` §22.1.0).

**This repo:** `ultramegausb-amiga` — see **[Amiga project status](#amiga-project-status-this-repo)** below for what is already done vs what still needs porting from Atari v22.1.0.

**Start here for implementation work:** [`doc/future_work.md`](./future_work.md) § Bluetooth pairing alignment (Atari v22.1.0).

---

## Ultramegausb family — what is usually the same

| Layer | Typical across adapters | Varies per product |
|-------|-------------------------|-------------------|
| **MCU / radio** | Raspberry Pi Pico 2 W, CYW43 BLE | Pico W (tighter RAM) |
| **Core 0** | TinyUSB host, Bluepad32, OLED UI, main loop | Input routing, shortcuts |
| **Core 1** | Tight timing loop from **XIP flash** | Atari: HD6301 emulator; **Amiga: quadrature mouse + GPIO bit-bang**; ADB: ADB bus timing |
| **BT stack** | pico-sdk BTstack + Bluepad32 platform callbacks | Device-type storage layout |
| **Flash** | BTstack TLV pairing keys + user **NVSettings** | Sector addresses per board |
| **Symptoms** | KB/mouse OK until BLE gamepad pairs; intermittent hang | Which gamepads trigger long pair |

If your adapter uses **dual-core + Core 1 in XIP + Bluepad32 on Core 0**, treat this document as directly applicable even when Core 1 is not an HD6301.

---

## Symptom pattern (what users reported)

- BLE **keyboard + mouse** work reliably.
- Pairing a **BLE HID gamepad** (Google Stadia, Xbox Wireless — CoD **0x0508**, HID-over-GATT) while KB/mouse are connected can:
  - **Hang the whole adapter** (OLED/UI frozen too → Core 0 blocked, not just Core 1).
  - Stop keyboard/mouse reaching the **host** (Atari serial, Amiga clock/data lines, ADB bus) even though the UI still updates.
- **Intermittent / “random”** — classic race with flash and multicore timing.
- **PS5 DualSense (BLE)** often unaffected; problem clustered on **longer pairing paths** (Xbox/Stadia bonding + TLV flash writes).
- Bug **vanished or moved** when verbose `printf` / `logi` was added (Heisenbug).

### Amiga-specific symptoms to watch

After pairing a BLE gamepad with BT keyboard + mouse already connected, verify on real hardware:

| Check | Pass criteria |
|-------|----------------|
| **Keyboard serial** | Amiga still receives key events (clock/data on Port 0) |
| **Quadrature mouse** | Port 1 mouse motion and buttons still work |
| **Joystick ports** | Port 1 (shared with mouse) and Port 2 still respond |
| **OLED** | UI not frozen; Map Devices shows gamepad name after pair |
| **Reboot** | Bonded devices reconnect without hang |
| **Clear keys** | Left+Right 5 s on splash (or equivalent) → fresh pair works |

---

## Architecture that makes this possible

| Piece | Role |
|-------|------|
| **Core 0** | TinyUSB, Bluepad32/BTstack, OLED, main loop (`src/main.c`) |
| **Core 1** | Timing-critical work from **XIP flash** — on Amiga: `amiga_quad_mouse_motion()` in `src/platform/amiga/quad_mouse.c` |
| **BTstack TLV** | Pairing keys / persistence written via **`flash_safe_execute()`** |
| **CYW43** | WiFi/BT chip; flash access must coordinate with **both cores** |

If Core 1 keeps executing from flash while Core 0/BTstack erases/writes flash, you get **stalls, corruption, or permanent freeze**. `flash_safe_execute()` tries to pause the other core — but only if that core cooperates.

**Amiga note:** Core 1 is a **quadrature mouse emulator** (not an HD6301), launched via `multicore_launch_core1(amiga_quad_mouse_motion)` from `quad_mouse.c`. It still runs from XIP and toggles Amiga GPIO for mouse/joystick Port 1. The same flash race applies.

---

## Root causes we confirmed (Atari v22.1.0)

### 1. Core 1 not enrolled in flash-safe multicore

**Fix:** Call `flash_safe_execute_core_init()` at the **start of Core 1** before any XIP work.

**Atari reference:** `src/main.cpp` → `core1_entry()`.  
**Amiga today:** ✅ Done — first call inside `amiga_quad_mouse_motion()` in `quad_mouse.c`.

### 2. Core 1 still in XIP during pairing flash writes

**Fix:** **Pause Core 1** before BTstack writes pairing data, resume only after enumeration completes.

- Pause on **gamepad discovery** (CoD `0x0508` or name match `Stadia` / `Xbox`), not on every BLE device.
- **BLE keyboards/mice** use a shorter path — do **not** pause Core 1 for them.
- Core 1 pause loop must **`__wfe()`** (not busy-spin forever) so multicore lockout / `flash_safe_execute` can preempt.

**Atari reference:** `main.cpp` + `bluepad32_platform.c` → `my_platform_on_device_discovered()`.  
**Amiga today:** ⚠️ Partial — pause on discovery exists (`amiga_quad_mouse_pause_core1()`), but pause loop uses `busy_wait_us(5000)` spin, **not `__wfe()`**. Core 1 also runs `sleep_us(50)` each loop iteration when not paused.

### 3. Single bool pause flag → refcount bugs

**Fix:** **Refcounted** `g_core1_pause_depth` — pause once on discovery; **do not pause again** in `on_device_connected`; resume once in `on_device_ready` (or on disconnect if pairing aborted).

**Amiga today:** ❌ Still a single `volatile bool g_core1_paused`. **Double-pause on connect** for Xbox/Stadia still present in `bluepad32_platform.c` → `my_platform_on_device_connected()`. Disconnect always calls `resume_core1()` with no depth check.

### 4. `sleep_ms()` / `__wfe()` inside Bluepad32 callbacks froze Core 0

**Fix:** In BT platform callbacks use **`busy_wait_us()` only** via `bt_callback_busy_wait_ms()`.

**Amiga today:** ❌ `sleep_ms(2000)` in `on_init_complete`; `sleep_ms(50)` / `sleep_ms(10)` in `on_device_ready` for gamepads. No `bt_callback_busy_wait_ms()` helper yet.

### 5. Resume too early (race masked by debug logging)

**Fix:** Explicit delays in `config.h`:
- `BT_GAMEPAD_DISCOVERY_SETTLE_MS` = **30**
- `BT_GAMEPAD_CORE1_RESUME_DELAY_MS` = **100**

Plus `core1_wait_for_pause_active()` after pausing.

**Amiga today:** ❌ No config constants. Ad-hoc 50 ms / 10 ms `sleep_ms` in `on_device_ready`. No settle delay after discovery pause. No `core1_wait_for_pause_active()`.

### 6. NVSettings / custom flash overlapping BTstack TLV bank

**Fix:** User flash sector must sit **below** the BTstack pairing TLV region; board-aware layout.

**Atari reference:** `NVSettings.cpp` — sector computed as `bt_bank - FLASH_SECTOR_SIZE`.  
**Amiga today:** ⚠️ **Needs audit** — `mouse_config.c` stores at `PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE` (last 4 KiB). BTstack `pico_flash_bank` also uses the **end of flash** (typically last 8 KiB). These may **overlap** — same class of bug Atari fixed. Validate against `PICO_FLASH_BANK_TOTAL_SIZE` for each board before next BT hardening pass.

### 7. Main-loop timing experiments broke pairing

Cherry-picking faster USB/HID polling caused Stadia/Xbox pairing hangs. Do not speed up Core 0 without retesting **KB + mouse + gamepad** pairing.

**Amiga today:** Main loop has no fixed 10 ms HID block (runs `tuh_task()` + `amiga_service()` as fast as possible). Document baseline before tuning. `bluepad32_poll()` called every iteration when BT enabled.

### 8. BTstack version pin

Stay on **pico-sdk–pinned BTstack** (v1.6.2 era) unless you port `hids_host` + Bluepad32 HID client changes.

**Amiga today:** Same submodule pin as siblings — see `doc/submodule-versions.md`.

### 9. Core 1 work gated on `absolute_time` after BT flash (Amiga, July 2026)

**Symptom:** After Stadia (or similar long BLE bond) pairs, Core 1 heartbeat still climbs, Core 0 still feeds mouse deltas, but **host motion dies**. Amiga DIAG: `motion_feeds↑`, `consumed=0`, `quad_gpio=0`, `flag=1`.

**Cause:** Quadrature consume/GPIO updates were inside `if (absolute_time_diff_us(...) >= period)`. After bond/`flash_safe_execute` lockout that gate stopped opening while the loop kept spinning.

**Fix:** Drive Core 1 periods with a **loop counter**; consume pending motion immediately; prefer `busy_wait_us` over `sleep_us` on Core 1. Shipped Amiga **v2.2.18**. Full write-up: [`stadia-controller-verification.md`](./stadia-controller-verification.md).

**Sibling note:** Atari Core 1 already avoids `absolute_time` for its heartbeat (“Use loop counter instead of absolute_time to avoid Bluetooth blocking” in `main.cpp`). Apple ADB should audit any Core 1 path that gates **output generation** on `get_absolute_time()` across BT flash. Pause/refcount alone does not cover this failure mode.

---

## What did *not* cause the hang (ruled out)

- Map Devices OLED / `usb_device_map` / device name strings (Amiga v2.1.1+).
- Stadia vs Xbox being “classic BR/EDR only” on Pico 2 W — captures showed **BLE HID gamepads**.
- Deferring OLED updates during Xbox/Stadia enumerate (Amiga lesson) — reduces interference but is **not** the root multicore flash race.
- **Rev 6 GPIO pin remap** — orthogonal to the Amiga `consumed=0` mouse-dead mode (failed the same on forced Rev 5).

---

## Checklist for this project (Amiga)

### Boot / multicore

- [x] `flash_safe_execute_core_init()` on Core 1 entry (`quad_mouse.c`)
- [x] Core 1 motion period **not** gated solely on `absolute_time` (v2.2.18 loop counter + immediate `motion_flag` consume)
- [ ] Pause branch policy: prefer `__wfe()` *or* documented busy-wait that cannot miss SEV after flash lockout (Amiga currently uses `busy_wait_us` in pause — intentional after WFE misses)
- [ ] Confirm wireless build XIP vs `copy_to_ram` in `CMakeLists.txt` (if applicable)

### Bluetooth platform callbacks (`src/bluepad32_platform.c`)

- [ ] Replace remaining `sleep_ms` in callbacks with `bt_callback_busy_wait_ms()` / `busy_wait_us`
- [x] Refcounted / anti-stack pause API + force-release / watchdog (bisect line; fold cleanly to main)
- [x] Heartbeat stall detection + SEV / relaunch (recovers brief park during Stadia bond)
- [ ] Align discovery settle / ready delays with Atari `BT_GAMEPAD_*_MS` where still ad-hoc
- [ ] Disconnect resume only when pause depth warrants it (review force-release vs Atari)

### Flash layout

- [ ] Audit `mouse_config.c` offset vs BTstack TLV bank (see §6 above)
- [ ] Align layout with Atari `NVSettings.cpp` pattern if overlap confirmed

### Clock / RF

- [ ] Amiga runs **200 MHz** (`src/CMakeLists.txt` `SYS_CLK_MHZ=200`) — Atari BT builds use **225 MHz**. Soak-test pairing at 200 MHz; consider 225 MHz trial if CYW43 stalls appear.

### Core 0 main loop (`src/main.c`)

- [ ] Document current loop timing before changes
- [ ] Retest pairing after any `tuh_task` / HID frequency change

### Diagnostics

- [x] `motion_feeds` / `consumed` / `quad_gpio` / `period` DIAG (prove consume path)
- [ ] Gate `[DIAG]` logs behind compile flag for release — they change timing

---

## Amiga file map (where to edit)

| File | Current role | Target (Atari v22.1.0 + Amiga lessons) |
|------|--------------|------------------------------------------|
| `src/platform/amiga/quad_mouse.c` | Core 1 loop, pause API, motion consume | Keep loop-counter period (v2.2.18); finish pause API polish |
| `src/bluepad32_platform.c` | BT callbacks, discovery pause | busy_wait only; settle/ready constants |
| `src/config.h` | GPIO, version, features | `BT_GAMEPAD_*_MS` where still missing |
| `src/platform/amiga/mouse_config.c` | Mouse type flash persistence | Re-validate sector vs BTstack TLV |
| `src/main.c` | Core 0 main loop | Heartbeat watchdog tick |
| `src/CMakeLists.txt` | 200 MHz overclock | Document; optional 225 MHz BT trial |

**Diff against Atari (canonical):**

```
ultramegausb-atari-st-rpikbd/src/main.cpp          → Core 1 pause/refcount/diagnostics
ultramegausb-atari-st-rpikbd/src/bluepad32_platform.c → callback timing + discovery/ready/disconnect
ultramegausb-atari-st-rpikbd/include/config.h      → BT_GAMEPAD_*_MS constants
ultramegausb-atari-st-rpikbd/src/NVSettings.cpp    → flash sector layout pattern
```

**Amiga Stadia/mouse consume lesson (port knowledge, not pin maps):**
[`doc/stadia-controller-verification.md`](./stadia-controller-verification.md)

---

## Amiga project status (this repo)

| Item | Status | Notes |
|------|--------|-------|
| `flash_safe_execute_core_init()` on Core 1 | ✅ | `quad_mouse.c` |
| Gamepad discovery pause | ⚠️ | Evolving; anti-stack + optional skip paths on bisect |
| Refcounted / force-release pause | ✅/⚠️ | Present on bisect line; land cleanly on main |
| Core 1 motion not `absolute_time`-gated | ✅ | **v2.2.18** — fixes Stadia→mouse `consumed=0` |
| Heartbeat stall SEV / relaunch | ✅ | Brief park during Stadia bond still possible |
| `busy_wait_us` only in BT callbacks | ⚠️ | Partial |
| Config delay constants | ⚠️ | Partial |
| Flash sector vs BTstack TLV | ⚠️ | `mouse_config.c` — audit required |
| Map Devices / UI during pair | ✅ | Ruled out as hang cause; defer OLED on Stadia/Xbox |
| Historical “Stadia fix” (pause-only) | ⚠️ | Necessary but **not** sufficient; see v2.2.18 |

**Conclusion:** Amiga now has the **consume-path** fix for post-Stadia dead mouse motion (v2.2.18) plus earlier flash-safe / pause work. Remaining work is polishing pause/callback timing and flash layout toward full Atari v22.1.0 alignment — not re-deriving the GPIO pin table as the mouse bug.

---

## Suggested porting order (this repo)

1. Read [`doc/future_work.md`](./future_work.md) § Bluetooth pairing alignment.
2. Land v2.2.18-class Core 1 consume fix on `main` if still on a bisect/feature branch.
3. **Audit flash map** — `mouse_config.c` vs BTstack TLV (`PICO_FLASH_BANK_TOTAL_SIZE`).
4. Finish **pause/callback polish** toward Atari (`BT_GAMEPAD_*_MS`, busy_wait-only callbacks).
5. Hardware matrix on **Pico 2 W**: BT KB + BT mouse connected → pair Stadia or Xbox → keyboard/mouse/joystick still work (`consumed` tracks feeds).
6. Only then tune main-loop timing or clock speed.
7. Propagate §9 knowledge to Atari/Apple docs if their Core 1 still gates host output on `absolute_time`.

---

## `__wfe()` — why Core 1 pause uses it

**Wait For Event** — ARM instruction that sleeps the CPU until an interrupt or `__sev()` wakes it.

Pico SDK `flash_safe_execute()` sends a **multicore lockout** FIFO IRQ to Core 1. The lockout handler internally uses `__wfe()` while flash erase/program runs on Core 0.

If Core 1’s **application pause loop** busy-spins instead of `__wfe()`:

- Core 1 keeps executing pause-loop code from XIP
- IRQ latency and flash bus contention can remain higher
- Lockout may not quiesce Core 1 as reliably during BTstack TLV writes

**Rule of thumb for this family:**

| Context | Use |
|---------|-----|
| Core 1 while `paused` | `__wfe()` (Atari default) — **or** short `busy_wait_us` if WFE has been observed to miss SEV after flash lockout (Amiga lesson) |
| Core 1 host-output period | **Loop counter**, not `get_absolute_time()` after BT flash (Amiga v2.2.18 / Atari heartbeat comment) |
| Core 0 inside BT callbacks (delays) | `busy_wait_us()` only |
| Core 0 inside BT callbacks (waiting for Core 1 paused) | `busy_wait_us()` poll (`core1_wait_for_pause_active`) |

Do **not** use `__wfe()` on Core 0 inside BT callbacks — it may never wake if no event is sent.

---

## References

### Atari repo (canonical fixes)

| Document | Content |
|----------|---------|
| `docs/BT_PAIRING_HANDOFF.md` | Generic family handoff (upstream of this file) |
| `RELEASE_NOTES.md` §22.1.0 | User-facing fix summary |
| `docs/FUTURE_WORK.md` | Regression lessons, BTstack pin |
| `docs/UI_UNIFICATION.md` | Shared OLED UI (Amiga design source) |

### This repo

| Document | Content |
|----------|---------|
| [`doc/future_work.md`](./future_work.md) | **Start here** — pairing alignment task list |
| [`doc/stadia-controller-verification.md`](./stadia-controller-verification.md) | Stadia USB formats + BLE/`consumed=0` mouse fix |
| [`doc/device_troubleshooting.md`](./device_troubleshooting.md) | User-facing BT/device issues |
| [`doc/todo.md`](./todo.md) | General project TODO |
| [`doc/submodule-versions.md`](./submodule-versions.md) | pico-sdk / bluepad32 pins |

---

## One-paragraph summary for paste into another LLM session

> **Context:** ultramegausb-amiga (Pico 2 W, Bluepad32, dual-core). Core 1 = quadrature mouse from XIP (`quad_mouse.c`). **Problems:** (1) BLE gamepad pairing hangs — multicore flash race during BTstack TLV; (2) after Stadia bond, mouse motion dead while Core 1 heartbeat alive — `motion_feeds↑` / `consumed=0` because consume was gated on `absolute_time` (fixed v2.2.18 with loop-counter period). **GPIO Rev 6 remap was not the mouse-dead cause.** **Port knowledge to siblings:** do not gate Core 1 host-output on `absolute_time` across BT flash; Atari already uses loop counters for Core 1 heartbeat; Apple ADB has pause/refcount — audit any absolute_time-gated output. **Read:** `doc/BT_PAIRING_HANDOFF.md` §9, `doc/stadia-controller-verification.md`, `doc/future_work.md`.
