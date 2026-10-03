# Ultrausbt family — Bluetooth pairing & gamepad best practices

**Audience:** LLM or developer working on any **ultrausbt** Pico / Pico 2 W HID adapter  
(Atari ST IKBD, Amiga keyboard/joystick, Apple ADB, or a future sibling).

**Purpose:** Standalone rules of thumb distilled from production debugging across the family.  
Copy this file into another repo’s `docs/` (or paste into an LLM session) without needing Amiga-specific context.

**Canonical code references (when available):**

| Project | Notes |
|---------|--------|
| `ultrausbt-atari-st-rpikbd` | v22.1.0+ pairing hardening (`main.cpp`, `bluepad32_platform.c`, `config.h`, `NVSettings.cpp`) |
| `ultrausbt-amiga` | v2.2.18+ Core 1 motion consume fix (`quad_mouse.c`); Stadia DIAG lessons |
| `ultrausbt-apple-adb-adapter` | Pause/refcount + `flash_safe_execute_core_init` patterns |

**Related deeper write-ups (optional):** project-local `archive/BT_PAIRING_HANDOFF.md`, Amiga `archive/stadia-controller-verification.md`.

---

## 1. Architecture you must assume

| Piece | Role |
|-------|------|
| **Core 0** | TinyUSB host, Bluepad32 / BTstack, OLED UI, main loop |
| **Core 1** | Timing-critical host I/O from **XIP flash** (Atari: HD6301; Amiga: quadrature mouse; ADB: bus timing) |
| **BTstack TLV** | Bonding keys persisted via **`flash_safe_execute()`** |
| **CYW43** | Radio; flash erase/program must coordinate with **both** cores |

If Core 1 keeps executing from XIP while Core 0 erases/writes flash, you get stalls, corruption, frozen UI, or “host input dies while OLED still updates.”

**Rule:** Treat every sibling as **dual-core + Core 1 in XIP + Bluepad32 on Core 0**, even when Core 1 is not an HD6301.

---

## 2. Symptom patterns (what to look for)

### Classic hang

- BLE keyboard + mouse work.
- Pairing a **BLE HID gamepad** (especially Stadia / Xbox Wireless, CoD ~`0x0508`) while KB/mouse are connected:
  - Freezes the whole adapter (UI too → Core 0 blocked), **or**
  - Stops host keyboard/mouse/joystick while UI still updates.
- Intermittent / “random.”
- Adding `printf` / verbose logs often **moves or hides** the bug (Heisenbug).

### Quiet death (Amiga-proven; watch for equivalents)

- Gamepad pairs successfully; buttons/axes may work on the gamepad path.
- **Mouse buttons still work; cursor/motion dead** (or Atari/ADB host motion/timing dead).
- Core 1 **heartbeat still climbing** (`paused=0`).
- Producer side still receiving HID deltas.

**Amiga DIAG signature:**

```text
motion_feeds↑  consumed=0  quad_gpio=0  flag=1  hb↑  paused=0
```

Interpretation: Core 0 feeds motion; Core 1 never runs the **consume / host-output** path.  
Do **not** assume “Core 1 is wedged” just because the host is dead — check whether the **work gate** still fires.

---

## 3. Non‑negotiable Core 1 / flash rules

### 3.1 Enrol Core 1 in flash-safe multicore

Call **once** at the start of the Core 1 entry function, before other XIP work:

```c
flash_safe_execute_core_init();
```

Without this, BTstack TLV / SSP writes can freeze or corrupt Core 1.

### 3.2 Pause Core 1 for heavy gamepad enumeration — carefully

- Pause on **gamepad discovery** (CoD `0x0508` / `0x2508`, or name match `Stadia` / `Xbox`), not on every BLE device.
- **Do not** pause Core 1 for ordinary BLE keyboards/mice (shorter path; pause hurts more than it helps).
- Use a **refcount / depth** (or “already paused → no-op”), **not** a bare bool you set twice and clear once.
- Pause **once** on discovery; **do not** pause again on `on_device_connected` for the same device.
- Resume **once** on `on_device_ready`, or force-release on disconnect / watchdog if pairing aborts without ready.
- After pause: wait until Core 1 is actually in the pause branch (`core1_wait_for_pause_active`), then settle (~**30 ms**).
- Before resume after gamepad ready: delay (~**100 ms**) with **`busy_wait_us` only** on Core 0.

Suggested config names (family convention):

```c
BT_GAMEPAD_DISCOVERY_SETTLE_MS      // typically 30
BT_GAMEPAD_CORE1_RESUME_DELAY_MS    // typically 100
BT_CORE1_PAUSE_WATCHDOG_MS          // force-release if enumerate aborts
```

### 3.3 What Core 1 does while paused

| Approach | When |
|----------|------|
| `__wfe()` | Atari default — lets multicore lockout / `flash_safe_execute` preempt cleanly |
| Short `busy_wait_us` spin | Acceptable if bare `__wfe()` has been observed to **miss SEV** after flash lockout (Amiga lesson) |

Never busy-spin for tens of ms with no yield if you can avoid it; never use `__wfe()` on **Core 0** inside Bluepad32 callbacks (may never wake).

### 3.4 Never sleep Core 0 inside Bluepad32 callbacks

In platform callbacks (`on_device_discovered` / `connected` / `ready` / etc.):

- Use **`busy_wait_us()`** / `bt_callback_busy_wait_ms()` only.
- **Do not** call `sleep_ms()`, `sleep_us()`, or `__wfe()` on Core 0 there — that freezes BTstack and looks like a “pairing hang.”

### 3.5 Do not gate Core 1 host-output on `absolute_time` across BT flash

**Critical Amiga lesson (v2.2.18):** After Stadia bond / `flash_safe_execute` lockout, Core 1 kept looping (heartbeat ↑) but this never ran:

```c
if (absolute_time_diff_us(last_update, now) >= period_us) {
    // consume motion / drive host pins
}
```

**Best practice:**

1. Drive Core 1 periods with a **loop counter** (or equivalent independent of a fragile time-gate), especially for anything that must keep producing host signals.
2. **Consume pending work immediately** when a cross-core flag is set; use the period tick for the state machine / pacing.
3. Prefer **`busy_wait_us`** over **`sleep_us`** on Core 1 after BT flash activity has been observed to hang sleeps.
4. Atari already documents loop-counter heartbeats “to avoid Bluetooth blocking” — keep that pattern; do not reintroduce `get_absolute_time()` as the sole gate for host I/O.

**Sibling audit question:** “Does any Core 1 path that generates host output only run inside an `absolute_time` comparison?” If yes, treat it as a latent Stadia/Xbox landmine.

### 3.6 Memory barriers are hygiene, not a substitute for the above

Use `__dmb()` / `__sync_synchronize()` / atomics for cross-core flags (`paused`, motion deltas, GPIO mode).  
They do **not** fix a time gate that never opens or a pause depth that never resumes.

### 3.7 Flash layout: user NV vs BTstack TLV

Keep user settings (mouse type, UI prefs, remaps) in a sector **that does not overlap** BTstack’s pairing TLV bank (usually near the **end of flash**).  
Atari pattern: compute user sector as `bt_bank - FLASH_SECTOR_SIZE`. Audit every sibling before shipping BT builds.

---

## 4. Controller matrix (what tends to hurt)

| Device | Typical path | Risk | Notes |
|--------|--------------|------|--------|
| **Google Stadia** `0x18D1` / `0x9400` | BLE HID gamepad, long bond / re-encrypt + TLV | **Highest** for Amiga mouse-dead + family hangs | Strongest trigger for flash lockout stress |
| **Xbox Wireless** `0x045E` | BLE HID gamepad, similar long path | High | Same pause / settle / resume class as Stadia |
| **PS5 DualSense** | Often shorter / different BLE path | Lower for *this* hang class | Still needs `flash_safe_execute` for SSP on some builds |
| **PS4 / generic Bluepad32 pads** | Varies | Medium | Retest after any Core 1 or callback change |
| **BLE mouse / keyboard** | Shorter GATT | Low for pause policy | **Do not** pause Core 1 on discovery for these |
| **USB gamepads** (Stadia USB, Xbox USB, etc.) | TinyUSB on Core 0 | Different class | Report descriptors / drivers; not the Core 1 flash race |

**Stadia-specific reminders:**

- USB report layouts (vigem `0x03` vs 9-byte) are **unrelated** to the BLE Core 1 flash/timing bugs.
- Stadia is a **trigger**, not a magic VID that “breaks GPIO maps.” Pin remaps and this failure mode are usually orthogonal.
- After pair, verify **host** paths (mouse motion, IKBD, ADB), not only that Bluepad32 says “device ready.”

---

## 5. Callback / UI rules of thumb

1. **Discovery:** if gamepad → pause Core 1 (if depth==0) → wait-for-pause → settle 30 ms.
2. **Connected:** do **not** pause again for Xbox/Stadia.
3. **Ready:** busy-wait resume delay 100 ms → resume once → then UI / storage updates.
4. **Disconnect / failed pair:** force-release pause if depth still > 0; arm a pause watchdog.
5. **Defer heavy OLED / flash UI** during Xbox/Stadia enumerate (helps; **not** a root-cause fix by itself).
6. Do not speed up Core 0 HID/`tuh_task` polling “for responsiveness” without retesting **KB + mouse + gamepad** pairing — timing experiments have reintroduced hangs.

---

## 6. Diagnostics worth shipping (even behind a flag)

Cross-core counters beat guesswork:

| Counter / field | Meaning |
|-----------------|--------|
| Core 1 heartbeat | Loop still running |
| Pause depth / paused flag | Intentionally stopped? |
| Phase (loop top / paused / work) | Where is Core 1? |
| Producer feed count | Core 0 posting work? |
| Consumer count | Core 1 taking work? |
| Host-output toggle count | Pins/protocol actually updating? |
| Last delta / pending flag | Stale handoff? |

**Healthy:** feed ≈ consume, host-output rising with input, pending flag clears.  
**Broken quiet death:** feed ↑, consume stuck, heartbeat ↑, paused=0.

Gate noisy `[DIAG]` prints behind a compile flag for release builds — logging changes timing.

Optional: if heartbeat freezes while not paused, SEV wake then **relaunch** Core 1 entry (Amiga) — recovers brief park during bond; still fix the underlying gate/pause bugs.

---

## 7. Hardware regression matrix (minimum)

Run on real Pico 2 W hardware after any BT / Core 1 change:

1. Boot → pairing window open.
2. Pair **BLE mouse** → move → host motion OK.
3. Pair **BLE keyboard** → keys OK; mouse still OK.
4. Pair **Stadia** (or Xbox Wireless) → gamepad host path OK.
5. Confirm mouse/keyboard **still** OK (the failure often appears here).
6. Reboot → bonded reconnect without hang.
7. Clear pairing keys → fresh pair still OK.
8. Repeat with **Stadia first**, then mouse (order used to matter; it must not).

Pass criteria: no UI freeze, no stuck pause depth, host motion/keys survive gamepad bond.

---

## 8. Anti‑patterns (do not do)

- Blame a **GPIO pin remap** for “mouse died after Stadia” without producer/consumer DIAG.
- “Fix” pairing with longer `sleep_ms` in Bluepad32 callbacks.
- Pause Core 1 for every BLE device including mice/keyboards.
- Double-pause on connect + single resume (orphan pause depth).
- Assume Core 1 is fine because heartbeat increments (check **consume / host-output**).
- Gate essential Core 1 host output solely on `get_absolute_time()` / `absolute_time_diff_us`.
- Overlap user flash settings with BTstack TLV banks.
- Treat OLED deferral or memory barriers as the complete fix.
- Cherry-pick faster USB loops without the pairing matrix above.

---

## 9. Quick checklist for a new sibling / LLM session

Copy and tick:

- [ ] `flash_safe_execute_core_init()` first in Core 1 entry
- [ ] Refcounted gamepad discovery pause; no connect double-pause
- [ ] Resume/force-release + pause watchdog
- [ ] `busy_wait_us` only in BT callbacks on Core 0
- [ ] Settle ~30 ms / resume delay ~100 ms (config constants)
- [ ] Core 1 host-output period uses **loop counter** (or equivalent), not fragile `absolute_time` gate
- [ ] Consume pending cross-core work promptly; barriers on handoff
- [ ] User NV flash sector audited vs BTstack TLV
- [ ] Stadia + Xbox in hardware matrix with KB/mouse already connected
- [ ] DIAG or equivalent can prove feed vs consume if motion/host I/O dies quietly

---

## 10. One-paragraph paste for another LLM

> Ultrausbt Pico 2 W adapters share Core 0 (Bluepad32/TinyUSB) + Core 1 (XIP timing: IKBD / Amiga quad mouse / ADB). BLE gamepads (especially Stadia `0x18D1/0x9400` and Xbox Wireless) stress BTstack TLV flash via `flash_safe_execute`. Required: `flash_safe_execute_core_init` on Core 1; refcounted pause on gamepad discovery only (not mice/KB); no double-pause; `busy_wait_us` only in BT callbacks; ~30 ms settle / ~100 ms resume; flash layout must not overlap TLV. Quiet failure mode: Core 1 heartbeat alive but host motion dead because work was gated on `absolute_time` after bond — use loop-counter periods and consume pending work immediately (Amiga v2.2.18). Do not blame GPIO remaps without feed/consume diagnostics. Retest KB+mouse+Stadia/Xbox on hardware after any Core 1 or callback change.

---

*Last updated: July 2026 — family lessons from Atari v22.1.0 pairing hardening and Amiga v2.2.18 Stadia→mouse consume fix.*
