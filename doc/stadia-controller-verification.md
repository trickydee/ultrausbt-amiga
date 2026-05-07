# Stadia Controller – Verification vs Atari Build

## Summary

The Amiga Stadia implementation was checked against the Atari IKBD build (`ultramegausb-atari-st-rpikbd`). The Amiga code now supports **both** report formats used or documented there.

## Atari Build Behaviour

1. **`stadia_controller.c` (C)**  
   - Uses a 9-byte payload: bytes 0–1 = buttons (16-bit), 2 = d-pad, 3–6 = sticks (Lx, Ly, Rx, Ry), 7–8 = triggers.  
   - **`stadia_process_report()` is never called** anywhere in the Atari tree. This layout is effectively unused for live input.

2. **`HidInput.cpp` (C++)**  
   - Stadia is read in `get_usb_joystick()` from the raw HID buffer using the **stadia-vigem** layout (from the stadia-vigem project).  
   - This is the format actually used for Stadia on Atari.

## Stadia Report Formats Supported (Amiga)

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

## Implementation Details

- **VID/PID:** `0x18D1` / `0x9400` (same as Atari; Atari uses `STADIA_CONTROLLER` = `0x9400`).
- **Direction:** Same bit layout as Atari: UP=0x01, DOWN=0x02, LEFT=0x04, RIGHT=0x08; d-pad takes priority over left stick.
- **Fire:** A/B/X/Y, R1, R2, or right trigger > 128 (Amiga also uses trigger; Atari uses face + triggers).
- **Deadzone:** 20 (same as Atari).
- **Unmount:** Amiga resets Joystick Port 2; Atari does not have an equivalent unmount handler in the Stadia C code.

## Files Compared

| Item              | Atari                         | Amiga                                      |
|-------------------|-------------------------------|--------------------------------------------|
| VID/PID           | 0x18D1, 0x9400                | Same                                       |
| Report parsing    | vigem in `HidInput.cpp` only  | vigem (0x03) + 9-byte with optional ID     |
| `stadia_process_report` | Present but never called | Called from `usb_hid.c`; supports both formats |
| Output            | Atari joystick axes/buttons   | Amiga Joystick Port 2 (direction + 3 buttons) |
| OLED on mount    | Raw hex dump in C; vigem in C++ | `display_show_controller_detected()`       |

## Conclusion

The Amiga implementation is aligned with the Atari build by:

1. Supporting the **stadia-vigem** report format (report ID `0x03`) that Atari actually uses in `HidInput.cpp`.
2. Keeping support for the 9-byte layout (with optional report ID) for compatibility with other possible Stadia report descriptors.
3. Using the same VID/PID, direction encoding, and deadzone as in the Atari code.

If a Stadia controller sends report ID `0x03` with the vigem layout, it will now be parsed correctly; if it sends the 9-byte layout (with or without a leading report ID), that path is still used.
