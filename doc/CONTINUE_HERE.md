# Continue here (session handoff)

**Last updated:** June 2026  
**Branch:** `feature/cd32`  
**Firmware:** v2.2.0  

---

## Latest (v2.2.0)

- **Port 1 CD32** — `cd32_pad.c` supports Port 1 and Port 2 (one active at a time).
- **OLED left button** — Port 1 cycle: **MOUSE → JOY → LLAMA → CD32 → MOUSE** (splash + Devices).
- **Flash persistence** — `port_config` saves Port 1 mode + Port 2 CD32 across reboots.
- **`port_mode.c`** — Central mode apply, mutual exclusion, keyboard chord integration.
- **BT Port 1 CD32** — Second gamepad via `port1_gamepad_submit()`.

### Still open
- USB gamepads → Port 1 CD32 (USB still routes to Port 2 only).
- `port2_gamepad_submit()` in PS3/PS5/Switch/PSC/HORI USB drivers.
- Full regression matrix in [`doc/CD32_BUILD_SPEC.md`](./CD32_BUILD_SPEC.md) §8.

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
