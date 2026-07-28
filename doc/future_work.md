# Future work & known limitations

**Last updated:** July 2026  
**Firmware:** v4.2.0 (`main`)  
**Purpose:** Open bugs, roadmap, and pointers to deeper archive notes — not a build checklist.

Public doc index: [`README.md`](./README.md). Architecture / agent quickstart: [`architecture.md`](./architecture.md). Historical task lists: [`archive/todo.md`](./archive/todo.md).

---

## Current status (v4.1.0)

Shipped and working on `main`:

| Area | Status |
|------|--------|
| USB/BT → Amiga (Device Mode) | Complete |
| Amiga kbd/mouse → PC (Host Mode) | Complete (keyboard + Port 1 mouse + Port 2 Atari 2-btn gamepad) |
| Dual joystick ports + OLED UI | Complete |
| **CD32 seven-button on Port 1 + Port 2** | **Complete and working** |
| Llamatron twin-stick | Complete (works with Port 2 CD32) |
| Settings carousel / USB role toggle | Complete (v4.1.0) |
| Atari/Amiga mouse feel tuning | Complete (v4.0.5–4.0.11) |

---

## Known issues

### Bluetooth gamepad slot routing on disconnect

Pads are assigned by **connection order** (`bt_gamepads[0]` → Port 2, `[1]` → Port 1). Powering off the first-paired pad can leave the survivor unrouted until re-pair. Prefer compacting slots on disconnect, or stable bind-by-address / “bind to port” UI (see UI roadmap).

This is a general dual-BT-pad routing issue — not a CD32 protocol bug.

### UART / serial log corruption

Observed causes / contributors:

1. **GPIO 1 (UART RX) wired while unused for host RX** — On Rev 6, GPIO **0/1** are reserved as `DEBUG_UART_TX` / `DEBUG_UART_RX`. If GPIO **1** is connected to something else (or tied into a shared net) while nothing is driving a clean UART RX line, serial output can corrupt or garble even though TX (GPIO 0) looks fine. Prefer leaving GPIO 1 **unconnected** unless a real debug UART host is attached; do not share it with Amiga I/O or other signals.
2. **Heavy IRQ / multi-context logging** — Under CD32 IRQ + Bluepad32 load, lines can also garble (`printf` / `logi` from multiple contexts). Release builds already gate most spam (`DEBUG_MESSAGES` / `CONTROLLER_DEBUG` off).

Longer-term software mitigations: serialize UART (ring buffer from main loop) and rate-limit Bluepad32 “Unsupported page” mouse logs (`0xff43`). When diagnosing garbled serial, first check whether GPIO 1 is connected without a proper UART peer.

### USB device mode (Amiga → PC) polish

Host Mode keyboard + Port 1 mouse + Port 2 Atari 2-button stick work as USB HID. Remaining polish:

- Mouse axis sign / button assignment if mirrored on some mice
- Mouse quadrature is polled from the main loop — IRQ/PIO if fast motion drops
- **Mega Drive pad input** — on hold (needs pin 5↔7 remapper); see [`host-mode-port2-joystick.md`](./host-mode-port2-joystick.md)
- **CD32 as USB HID gamepad to the PC** — not implemented
- Detail / diagnostics: [`archive/amiga-usb-device-mode.md`](./archive/amiga-usb-device-mode.md)

---

## Roadmap

### Bluetooth / Core 1

Much of the Atari v22.1.0 pairing work is already on Amiga (Core 1 loop-counter consume, pause/watchdog). Remaining polish:

- Flash-layout audit: `mouse_config` sector vs BTstack TLV (`PICO_FLASH_BANK_TOTAL_SIZE`)
- `bluepad32_platform.c` settle/ready delays toward Atari (`BT_GAMEPAD_*_MS`)
- Re-run KB + mouse + Stadia/Xbox pair matrix after any SDK / Bluepad32 bump
- Compact BT gamepad slots on disconnect (see known issue above)

See [`BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md) and [`archive/BT_PAIRING_HANDOFF.md`](./archive/BT_PAIRING_HANDOFF.md) / [`archive/stadia-controller-verification.md`](./archive/stadia-controller-verification.md).

### UI

| Item | Notes |
|------|-------|
| Cycle gamepad bindings on Map Devices | Atari UI unification Phase 2; helps dual-pad reconnect routing |
| Portable OLED spec | Local: `local/ULTRAMEGAUSB_OLED_UI_SPEC.md` (gitignored) |

### Hardware / firmware

| Item | Notes |
|------|-------|
| Rev 6 shipping | Port 2 fire/B2/B3 on GPIO **16/17/18**; OLED buttons on ADC **26/27/28**; UART on 0/1 — [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md) |
| USB gamepad → Port 1 | First USB pad still drives Port 2 by default; Port 1 CD32 is typically a second BT pad (or dual-pad setup) |
| Host Mode Port 2 Atari stick | Atari 2-btn → USB gamepad on `feature/host-mode-port2-joystick`; Mega Drive still on hold — [`host-mode-port2-joystick.md`](./host-mode-port2-joystick.md) |
| Submodule / SDK bump | Re-test BT + mouse after pico-sdk / bluepad32 updates ([`submodule-versions.md`](./submodule-versions.md)) |

---

## Resolved (reference)

| Item | Note |
|------|------|
| **CD32 Port 1 + Port 2 seven-button protocol** | Complete — OLED + shortcuts; ghost-button fix (CLOCK-fall shift) |
| OLED Settings carousel + Host/Device Mode | v4.1.0 |
| Atari/Amiga quadrature mouse feel | v4.0.5–4.0.11 |
| OLED Up/Down/# → Left/Right/Middle remap | v4.0.12 |
| USB device mode (Amiga kbd/mouse → PC) | v3.2.0+; Caps Lock macOS hold in v3.2.1 |
| Core 1 mouse dead after Stadia bond | Fixed v2.2.18 — do not gate consume on `absolute_time` |
| Map Devices UI + `build-all.sh` | v2.1.1 |
| Dual CD32 Port 1 + Port 2 wiring/protocol | v2.2.x onward |
| Release hygiene (UART quiet + LICENSE/NOTICE) | v4.0.0 |

---

## Documentation map

| Document | Role |
|----------|------|
| [`README.md`](./README.md) (this folder) | Public doc index |
| [`../README.md`](../README.md) | Product overview, shortcuts, build |
| [`architecture.md`](./architecture.md) | Software architecture + developer/agent quickstart |
| [`BT_PAIRING_BEST_PRACTICES.md`](./BT_PAIRING_BEST_PRACTICES.md) | BT pairing practices |
| [`gpio_rev6_adc_avoidance.md`](./gpio_rev6_adc_avoidance.md) | Rev 6 pin rationale |
| [`submodule-versions.md`](./submodule-versions.md) | Pinned SDK / Bluepad32 / TinyUSB |
| [`archive/`](./archive/) | Build specs, troubleshooting, upstream borb notes, WIP |
