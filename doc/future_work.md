# Future work & known limitations

**Last updated:** July 2026  
**Firmware:** v4.1.0 (`feature/releasecandidate` / `main`)  
**Purpose:** Open bugs, roadmap, and pointers to deeper archive notes — not a build checklist.

Public doc index: [`README.md`](./README.md). Historical task lists: [`archive/todo.md`](./archive/todo.md).

---

## Known issues

### CD32 pad mode

**CD32 seven-button protocol works on both Port 1 and Port 2** (OLED / shortcuts). Remaining rough edge:

1. **BT gamepad slot routing on disconnect** — Pads are assigned by **connection order** (`bt_gamepads[0]` → Port 2, `[1]` → Port 1). Powering off the first-paired pad can leave the survivor unrouted until re-pair. Prefer compacting slots on disconnect, or stable bind-by-address / “bind to port” UI.

Protocol/pin detail: [`archive/CD32_BUILD_SPEC.md`](./archive/CD32_BUILD_SPEC.md).

### UART / serial log corruption

Under heavy CD32 IRQ + Bluepad32 load, lines can garble (`printf` / `logi` from multiple contexts). Release builds already gate most spam (`DEBUG_MESSAGES` / `CONTROLLER_DEBUG` off). Longer-term: serialize UART (ring buffer from main loop) and rate-limit Bluepad32 “Unsupported page” mouse logs (`0xff43`).

### USB device mode (Amiga keyboard → PC)

Working on real hardware (A2000 keyboard → Rev 5 → macOS). Open polish:

- Mouse axis sign / button assignment if mirrored on some mice
- Mouse quadrature is polled from the main loop — IRQ/PIO if fast motion drops
- Joystick-as-HID-gamepad to the PC not implemented
- Detail / diagnostics: [`archive/amiga-usb-device-mode.md`](./archive/amiga-usb-device-mode.md)

---

## Roadmap

### Bluetooth / Core 1

Much of the Atari v22.1.0 pairing work is already on Amiga (Core 1 loop-counter consume, pause/watchdog). Remaining polish:

- Flash-layout audit: `mouse_config` sector vs BTstack TLV (`PICO_FLASH_BANK_TOTAL_SIZE`)
- `bluepad32_platform.c` settle/ready delays toward Atari (`BT_GAMEPAD_*_MS`)
- Re-run KB + mouse + Stadia/Xbox pair matrix after any SDK / Bluepad32 bump

See [`BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md) and [`archive/BT_PAIRING_HANDOFF.md`](./archive/BT_PAIRING_HANDOFF.md) / [`archive/stadia-controller-verification.md`](./archive/stadia-controller-verification.md).

### UI

| Item | Notes |
|------|-------|
| Cycle gamepad bindings on Map Devices | Atari UI unification Phase 2 |
| Portable OLED spec | Local: `local/ULTRAMEGAUSB_OLED_UI_SPEC.md` (gitignored) |

### Hardware / firmware

| Item | Notes |
|------|-------|
| Rev 6 shipping | Port 2 fire/B2/B3 on GPIO **16/17/18**; OLED buttons on ADC **26/27/28**; UART on 0/1 — [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md) |
| USB gamepad → Port 1 | First USB pad still drives Port 2 by default; Port 1 CD32 is typically a second BT pad (or dual-pad setup) |
| Submodule / SDK bump | Re-test BT + mouse after pico-sdk / bluepad32 updates ([`submodule-versions.md`](./submodule-versions.md)) |

---

## Resolved (reference)

| Item | Note |
|------|------|
| USB device mode (Amiga kbd/mouse → PC) | v3.2.0+; Caps Lock macOS hold in v3.2.1 |
| Core 1 mouse dead after Stadia bond | Fixed v2.2.18 — do not gate consume on `absolute_time` |
| Map Devices UI + `build-all.sh` | v2.1.1 |
| Dual CD32 Port 1 + Port 2 | v2.2.x |
| CD32 ghost adjacent buttons | Fixed (CLOCK-fall shift / DATA stable before Amiga sample) |
| Release hygiene (UART quiet + LICENSE/NOTICE) | v4.0.0 |

---

## Documentation map

| Document | Role |
|----------|------|
| [`README.md`](./README.md) (this folder) | Public doc index |
| [`../README.md`](../README.md) | Product overview, shortcuts, build |
| [`BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md) | BT pairing practices |
| [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md) | Rev 6 pin rationale |
| [`submodule-versions.md`](./submodule-versions.md) | Pinned SDK / Bluepad32 / TinyUSB |
| [`archive/`](./archive/) | Build specs, troubleshooting, upstream borb notes, WIP |
