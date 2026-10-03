# Stadia Controller – Verification Notes

## Summary

Covers two separate topics:

1. **USB HID report formats** — Amiga vs Atari IKBD parsing (unchanged).
2. **BLE pairing + Amiga quadrature mouse** — July 2026 findings (v2.2.13–v2.2.18). This is the important operational knowledge for Stadia on Pico 2 W.

Related: [`device_troubleshooting.md`](./device_troubleshooting.md) (Stadia section), [`BT_PAIRING_HANDOFF.md`](./BT_PAIRING_HANDOFF.md).

---

## BLE pairing + mouse motion (July 2026)

### Symptom

After a Google Stadia controller pairs (or re-bonds) over BLE, **mouse buttons still work** but **cursor motion dies**. Stadia directions/buttons on Port 2 remain OK. Order often looks like: Stadia first → mouse connects → no movement; or mouse worked alone until Stadia paired.

Core 0 keeps receiving HID motion. The Amiga never sees quadrature edges.

### What DIAG proved (v2.2.16–v2.2.17)

Periodic `[DIAG] Core1 …` lines (Core 0 watchdog) showed:

| Field | Broken | Healthy (v2.2.18+) |
|-------|--------|---------------------|
| `hb` | Climbing | Climbing |
| `paused` / `bt_depth` | `0` | `0` |
| `port1` | `MOUSE` | `MOUSE` |
| `motion_feeds` | Rising with movement | Rising |
| `consumed` | **Stuck at 0** | Tracks `motion_feeds` |
| `quad_gpio` | **Stuck at 0** | Rising |
| `flag` | Stuck `1` | Clears to `0` between moves |
| `period` | (added in v2.2.18) | Climbing even at idle |

So Core 1 was **alive** and Core 0 was **feeding** motion, but Core 1 never entered the path that clears `motion_flag` and drives Port 1 quadrature GPIOs.

### Root cause (Amiga-specific)

Motion consume + quad GPIO updates were gated on:

```c
if (absolute_time_diff_us(last_update, current_time) >= update_period_us) {
    // consume motion_flag, update quadrature GPIOs
}
```

After Stadia bond/flash activity (`flash_safe_execute` lockout on Core 1), that **time gate stopped opening** while the loop itself kept running (`busy_wait_us` / heartbeat). Result: `feeds↑`, `consumed=0`, dead cursor.

**Not** the primary cause:

- Rev 6 GPIO pin remap (Port 2 fire/B2/B3) — does not sit on the mouse quadrature path; Rev 5 forced builds still failed the same way.
- Missing `__dmb()` alone — barriers are good hygiene for the Core0↔Core1 handoff, but DIAG showed the timed update block never ran.
- “Stadia report format” — BT path uses Bluepad32; USB report layout is unrelated.

Stadia is a strong **trigger** because its BLE bond/re-encrypt path does more TLV flash work than a typical mouse pair. Mouse-only sessions often never hit the failure.

### Fix (v2.2.18)

In `src/platform/amiga/quad_mouse.c` (`amiga_quad_mouse_motion`):

1. Drive the update period with a **loop counter** (~170 µs via `busy_wait_us(50)` × N), not `get_absolute_time()`.
2. **Consume `motion_flag` immediately** when set (still tick accumulators on the period).
3. Keep `__dmb()` around the `x` / `y` / `motion_flag` handoff.
4. Prefer `busy_wait_us` over `sleep_us` on Core 1 after BT flash activity.

Healthy check: `consumed` ≈ `motion_feeds`, `quad_gpio` rising, `period` climbing, `flag=0` at idle.

### Residual behaviour

During Stadia setup you may still see:

```text
[BT] Core 1 heartbeat stalled … — SEV wake
[BT] Core 1 still stalled — relaunching amiga_quad_mouse_motion
```

Flash lockout can briefly park Core 1; the heartbeat watchdog relaunches it. After v2.2.18, mouse motion recovers. Cleaning that stall is separate from the `consumed=0` bug.

### Correlation with the GPIO branch

The failure was found while bringing up Rev 6 GPIO maps, which made it easy to blame the pin table. The GPIO commit only retargeted `#if HIDPICO_REVISION == 5` → `HIDPICO_REV_ATARI_BOARD` in `quad_mouse.c`. The latent bug was the **absolute_time-gated consume path** under Stadia/flash stress (already fragile after earlier BLE hardening). See also discussion in session notes: pin remap ≠ timer gate failure.

### Quick regression matrix

| Step | Expect |
|------|--------|
| Banner `v2.2.18+ (PCB rev 5)` | Correct build |
| Pair Stadia → move sticks/buttons | Port 2 OK |
| Pair BT mouse → move | Cursor moves; DIAG `consumed` rises |
| Pair KB after | Keys OK; mouse still moves |
| Reboot with bonds | Reconnect; mouse still moves |

---

## USB report formats (Amiga vs Atari)

The Amiga Stadia USB implementation was checked against the Atari IKBD build (`ultrausbt-atari-st-rpikbd`). The Amiga code supports **both** report formats used or documented there.

### Atari build behaviour

1. **`stadia_controller.c` (C)**  
   - Uses a 9-byte payload: bytes 0–1 = buttons (16-bit), 2 = d-pad, 3–6 = sticks (Lx, Ly, Rx, Ry), 7–8 = triggers.  
   - **`stadia_process_report()` is never called** anywhere in the Atari tree. This layout is effectively unused for live input.

2. **`HidInput.cpp` (C++)**  
   - Stadia is read in `get_usb_joystick()` from the raw HID buffer using the **stadia-vigem** layout (from the stadia-vigem project).  
   - This is the format actually used for Stadia on Atari.

### Format 1: stadia-vigem (matches Atari `HidInput.cpp`)

- **Report ID:** `0x03` (first byte).
- **Length:** ≥ 11 bytes (1 byte ID + 10 payload).
- **Layout:**
  - Byte 0: `0x03` (header)
  - Byte 1: D-Pad hat (0–7; 8/15 = centre)
  - Byte 2: System buttons (Options, Menu, Stadia, etc.)
  - Byte 3: Face/shoulder (bit 6=A, 5=B, 4=X, 3=Y, 2=LB, 1=RB, 0=LS)
  - Bytes 4–5: Left stick X, Y (0–255, 128 centre)
  - Bytes 6–7: Right stick X, Y
  - Bytes 8–9: Left trigger, Right trigger

### Format 2: 9-byte payload (optional report ID)

- **Report ID:** Optional; if present, first byte is 1–15 (e.g. `0x01`).
- **Length:** ≥ 10 bytes with ID, or ≥ 9 without.
- **Payload (after optional ID):**
  - Bytes 0–1: Buttons (16-bit)
  - Byte 2: D-Pad (0–8; 15 = neutral)
  - Bytes 3–6: Left X, Left Y, Right X, Right Y
  - Bytes 7–8: Left trigger, Right trigger

### Implementation details

- **VID/PID:** `0x18D1` / `0x9400` (same as Atari; Atari uses `STADIA_CONTROLLER` = `0x9400`).
- **Direction:** Same bit layout as Atari: UP=0x01, DOWN=0x02, LEFT=0x04, RIGHT=0x08; d-pad takes priority over left stick.
- **Fire:** A/B/X/Y, R1, R2, or right trigger > 128 (Amiga also uses trigger; Atari uses face + triggers).
- **Deadzone:** 20 (same as Atari).
- **Unmount:** Amiga resets Joystick Port 2; Atari does not have an equivalent unmount handler in the Stadia C code.

### Files compared

| Item              | Atari                         | Amiga                                      |
|-------------------|-------------------------------|--------------------------------------------|
| VID/PID           | 0x18D1, 0x9400                | Same                                       |
| Report parsing    | vigem in `HidInput.cpp` only  | vigem (0x03) + 9-byte with optional ID     |
| `stadia_process_report` | Present but never called | Called from `usb_hid.c`; supports both formats |
| Output            | Atari joystick axes/buttons   | Amiga Joystick Port 2 (direction + 3 buttons) |
| OLED on mount    | Raw hex dump in C; vigem in C++ | `display_show_controller_detected()`       |

### USB conclusion

The Amiga USB implementation is aligned with the Atari build by supporting the **stadia-vigem** report format (report ID `0x03`) plus the 9-byte layout, with the same VID/PID, direction encoding, and deadzone.
