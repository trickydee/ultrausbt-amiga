# Continue here (session handoff)

**Last updated:** June 2026  
**Branch:** `feature/cd32`  
**Firmware:** v2.2.5  

---

## Latest (v2.2.5)

- **CD32 timing:** Restored v2.1.2 (`97a2c87`) IRQ state machine per port — `configure_clock_for_joymode()` on every JOYMODE edge, CLOCK rise IRQ always enabled, latch only on JOYMODE fall. Dual-port structure unchanged.

## v2.2.4

- **CD32 timing:** Shift advance on CLOCK **rise** again (opposite of v2.2.3 fall experiment); JOYMODE dumb-mode still immediate in ISR.

## v2.2.3

- **CD32 timing:** Present next shift bit on CLOCK **fall** (Amiga samples on rise) — should reduce adjacent-button ghosts (B+A, Y+G).
- **JOYMODE rise:** Dumb-mode Red/Blue driven immediately in ISR (no main-loop deferral).

## v2.2.2

- **Fix:** CD32 GPIO IRQ handlers were too heavy (reconfiguring pins inside ISR) — starved main loop → OLED frozen, keyboard queue full (`unable to enqueue KeyDown`), no Amiga output.
- **Fix:** GPIO watchdog disabled while CD32 active (was resetting clock pins driven as OUTPUT).
- ISR now only toggles DATA via `gpio_put`; JOYMODE transitions deferred to `cd32_service()` in main loop.

## v2.2.1

- **Dual CD32** — Port 1 and Port 2 CD32 can run **simultaneously** (independent shift-register state per port).
- **Typical setup:** OLED cycle Port 1 to CD32; **Shift + Left Amiga + C** for Port 2 CD32; BT pad #1 → Port 2, BT pad #2 → Port 1.
- **Port 1 mouse** unavailable while Port 1 CD32 active (Core 1 paused — expected).

## v2.2.0

- **Port 1 CD32** — `cd32_pad.c` supports Port 1 and Port 2.
- **OLED left button** — Port 1 cycle: **MOUSE → JOY → LLAMA → CD32 → MOUSE** (splash + Devices).
- **Flash persistence** — `port_config` saves Port 1 mode + Port 2 CD32 across reboots.
- **`port_mode.c`** — Central mode apply; Llamatron disabled when either CD32 port active.
- **BT Port 1 CD32** — Second gamepad via `port1_gamepad_submit()`.

### Still open
- **CD32 timing:** ghost adjacent buttons (B+A, Y+G) — shift-register DATA/CLOCK presentation (`doc/future_work.md`).
- **BT slot routing:** pad disconnect leaves survivor unrouted; reconnect order changes port (`doc/future_work.md`).
- **UART corruption:** garbled serial when busy — concurrent logging + Bluepad32 mouse vendor-page spam (`doc/future_work.md`).
- USB gamepads → Port 1 CD32 (USB still routes to Port 2 only).
- `port2_gamepad_submit()` in PS3/PS5/Switch/PSC/HORI USB drivers.
- Full regression matrix in [`doc/CD32_BUILD_SPEC.md`](./CD32_BUILD_SPEC.md) §8.

### Known instability (v2.2.2)
Dual CD32 is usable but not fully stable: occasional ghost button pairs on shift-register ports; BT pad routing breaks if one of two paired pads disconnects until re-pair in correct order; UART debug output can corrupt under load (reboot clears).

---

## Build

```bash
BUILD_BOARDS=pico2_w ./build.sh
```

Flash `dist/amigahid_pico2_w.uf2`.

## Controls (Rev 5)

| Action | Input |
|--------|-------|
| Port 1 mode cycle | OLED **left button** (splash or Devices) |
| Port 1 mouse ↔ joy | **Shift + Left Amiga + J** |
| Llamatron | **Shift + Left Amiga + L** |
| Port 2 CD32 | **Shift + Left Amiga + C** |

Modes persist across reboot.
