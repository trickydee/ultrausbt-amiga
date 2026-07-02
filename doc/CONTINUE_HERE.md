# Continue here (session handoff)

**Last updated:** June 2026  
**Branch:** `feature/cd32`  
**Firmware:** v2.1.2  
**Design reference (Atari):** `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultramegausb-atari-st-rpikbd/docs/UI_UNIFICATION.md`

Read this file first when resuming work on the Amiga adapter.

---

## What was completed (CD32 — v2.1.2)

### Port 2 CD32 gamepad protocol (Rev 5)
- **`src/platform/amiga/cd32_pad.c`** — JOYMODE latch + CLOCK shift ISRs; dumb 2-button mode when JOYMODE high.
- **`src/platform/amiga/port2_gamepad.c`** — `port2_gamepad_submit()` routes standard 3-button vs CD32 seven-button mapping.
- **Toggle:** **Shift + Left Amiga + C** (USB + BT keyboards); OLED Devices footer `Port2: CD32` / `Port2: STD`.
- **Drivers via `port2_gamepad_submit()`:** PS4, Xbox, Stadia (USB), generic HID, BT gamepad #0.
- **Protocol fixes:** Release CLOCK before JOYMODE latch; DATA always OUTPUT (KTRL-CD32 style).
- **Tested:** amiga-test-kit CD32 pad test; *Rainbow Islands* with BT Stadia and PS5.

### Still open (CD32)
- Flash persistence for CD32 mode flag.
- Route PS3, PS5, Switch, PSC, HORI USB drivers through `port2_gamepad_submit()` when CD32 on.
- Full regression matrix in [`doc/CD32_BUILD_SPEC.md`](./CD32_BUILD_SPEC.md) §8.
- Port 1 CD32 (deferred).

---

## Prior session (Map Devices — v2.1.1)

- Map Devices screen, `usb_device_map`, build-all.sh / `dist/` layout — see git history on `feature/ui-alignment`.

---

## Build commands

```bash
./build-all.sh                                    # default: Pico 2 W only
BUILD_BOARDS=pico,pico2_w ./build.sh              # Pico + Pico 2 W
BUILD_BOARDS=all ./build-all.sh                   # all boards
CLEAN_BUILD_DIRS=0 ./build-all.sh                 # keep build trees
```

Flash from **`dist/`** (e.g. `dist/amigahid_pico2_w.uf2`).

---

## Key source files (CD32)

| Area | Files |
|------|-------|
| CD32 protocol | `src/platform/amiga/cd32_pad.c`, `cd32_pad.h` |
| Gamepad routing | `src/platform/amiga/port2_gamepad.c`, `port2_gamepad.h` |
| USB/BT integration | `src/usb_hid.c` |
| USB drivers (CD32-aware) | `src/usb_controllers/ps4_controller.c`, `xbox_controller.c`, `stadia_controller.c` |
| Build spec | `doc/CD32_BUILD_SPEC.md` |

---

## Suggested next steps

1. Merge `feature/cd32` after review.
2. Add flash persist for CD32 toggle (`mouse_config.c` or new sector — audit vs BTstack TLV).
3. Port remaining USB controller drivers to `port2_gamepad_submit()`.
4. P2: Bluetooth pairing alignment — [`doc/BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md).
