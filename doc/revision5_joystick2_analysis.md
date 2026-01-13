# Revision 5 Joystick Port 2 Implementation Analysis

## Summary

**Status:** Joystick Port 2 is **NOT IMPLEMENTED** in the codebase. The code only handles keyboard and mouse (Port 1).

**Revision 5 Configuration:** Revision 5 GPIO configuration does **NOT EXIST** in `config.h`. Only Revision 2 and Revision 4 are currently defined.

---

## Revision 5 GPIO Pin Assignments (from KiCad netlist)

### Keyboard Interface (Revision 5)
Same as Revision 4:
- **GPIO 4** - Reset (`KBD_AMIGA_RST`)
- **GPIO 5** - Data (`KBD_AMIGA_DAT`)
- **GPIO 6** - Clock (`KBD_AMIGA_CLK`)

### Mouse/Joystick Port 1 (Revision 5)
Same as Revision 4:
- **GPIO 7** - Horizontal Quadrature (`QM1_AMIGA_HQ`)
- **GPIO 8** - Vertical Quadrature (`QM1_AMIGA_VQ`)
- **GPIO 9** - Horizontal (`QM1_AMIGA_H`)
- **GPIO 10** - Vertical (`QM1_AMIGA_V`)
- **GPIO 11** - Button 1 (`QM1_AMIGA_B1`)
- **GPIO 12** - Button 2 (`QM1_AMIGA_B2`)
- **GPIO 13** - Button 3 (`QM1_AMIGA_B3`)

### Joystick Port 2 (Revision 5) - **FIXED FROM REVISION 4**
The errata notes that Revision 4 erroneously used physical pin 30 (RUN) for the horizontal/down line. This was corrected in Revision 5:

| Function | GPIO Pin | Physical Pin | Signal Name | Notes |
|----------|----------|--------------|-------------|-------|
| Button 1 (Fire) | 20 | 26 | `/lcont2 b1` | |
| Button 2 | 19 | 25 | `/lcont2 b2` | |
| Button 3 | 18 | 24 | `/lcont2 b3` | |
| Horizontal Quadrature | 21 | 27 | `/lcont2 hq` | |
| Vertical Quadrature | 22 | 29 | `/lcont2 vq` | |
| Horizontal | 26 | 31 | `/lcont2 h` | **FIXED** - GPIO26_ADC0 |
| Vertical | 27 | 32 | `/lcont2 v` | **FIXED** - GPIO27_ADC1 |

**Key Fix:** The horizontal and vertical signals were moved from the erroneous pin 30 (RUN) to physical pins 31 (GPIO 26) and 32 (GPIO 27) respectively.

### I2C Display (Revision 5)
Same as Revision 4:
- **GPIO 2** - SDA (`I2C_PIN_SDA`)
- **GPIO 3** - SCL (`I2C_PIN_SCL`)

---

## Current Code Status

### 1. `src/config.h`
- **Missing:** Revision 5 configuration block
- **Present:** Revision 2 and Revision 4 only
- **Action Required:** Add `#elif HIDPICO_REVISION == 5` block with GPIO definitions

### 2. Joystick Port 2 Implementation
- **Status:** **NOT IMPLEMENTED**
- **Files Checked:**
  - `src/usb_hid.c` - Only handles keyboard and mouse
  - `src/platform/amiga/` - Only contains `keyboard_serial_io.c` and `quad_mouse.c`
  - No joystick/gamepad handling code exists

### 3. USB HID Support
- **Current:** Only `HID_ITF_PROTOCOL_KEYBOARD` and `HID_ITF_PROTOCOL_MOUSE` are handled
- **Missing:** Gamepad/joystick protocol handling
- **Note:** `tusb_config.h` defines `CFG_TUH_HID 4` which should support joysticks, but no code processes them

---

## What Needs to Be Implemented

### 1. Add Revision 5 Configuration to `config.h`

```c
#elif HIDPICO_REVISION == 5
#  define I2C_PORT      i2c1
#  define I2C_PIN_SDA   2
#  define I2C_PIN_SCL   3
#  define I2C_IRQN      24

#  define KBD_AMIGA_RST 4
#  define KBD_AMIGA_DAT 5
#  define KBD_AMIGA_CLK 6

#  define QM1_AMIGA_HQ  7
#  define QM1_AMIGA_VQ  8
#  define QM1_AMIGA_H   9
#  define QM1_AMIGA_V   10
#  define QM1_AMIGA_B1  11
#  define QM1_AMIGA_B2  12
#  define QM1_AMIGA_B3  13

// Joystick Port 2 (Revision 5 - fixed from Revision 4)
#  define QM2_AMIGA_B1  20
#  define QM2_AMIGA_B2  19
#  define QM2_AMIGA_B3  18
#  define QM2_AMIGA_HQ  21
#  define QM2_AMIGA_VQ  22
#  define QM2_AMIGA_H   26  // Fixed from Revision 4 error (was pin 30/RUN)
#  define QM2_AMIGA_V   27  // Fixed from Revision 4 error
#endif
```

### 2. Create Joystick Port 2 Implementation

**Required Files:**
- `src/platform/amiga/joystick_port2.c` - GPIO control and state management
- `src/platform/amiga/joystick_port2.h` - Public API

**Required Functions:**
- `amiga_joystick_port2_init()` - Initialize GPIO pins
- `amiga_joystick_port2_set_direction()` - Set joystick direction (up/down/left/right)
- `amiga_joystick_port2_set_button()` - Set button state (fire, button 2, button 3)

### 3. Add Gamepad/Joystick Handling to USB HID

**In `src/usb_hid.c`:**
- Add handler for gamepad/joystick HID reports
- Map USB gamepad inputs to Amiga joystick port 2 signals
- Handle direction (D-pad or analog stick) and buttons

**Note:** The Amiga joystick port uses simple digital signals (active low):
- 4 directions: Up, Down, Left, Right (via H and V signals)
- Quadrature signals (HQ, VQ) for mouse compatibility
- 3 buttons: Fire (B1), Button 2 (B2), Button 3 (B3)

---

## Verification Checklist

- [ ] Revision 5 GPIO configuration added to `config.h`
- [ ] Joystick Port 2 GPIO pins match KiCad netlist (GPIO 18, 19, 20, 21, 22, 26, 27)
- [ ] Joystick Port 2 initialization code created
- [ ] Joystick Port 2 GPIO control functions implemented
- [ ] USB HID gamepad/joystick handler added
- [ ] USB gamepad input mapped to Amiga joystick port 2 signals
- [ ] Testing with actual hardware

---

## References

- `src/config.h` - Current GPIO configuration
- `kicad/amigahid-pico.net` - KiCad netlist with Revision 5 pin assignments
- `doc/errata.md` - Hardware errata documenting Revision 4 error
- `src/platform/amiga/quad_mouse.c` - Reference implementation for Port 1 (similar structure needed for Port 2)
- `src/usb_hid.c` - USB HID event handling (needs gamepad support added)

---

## Conclusion

**Joystick Port 2 is completely unimplemented.** The GPIO pins for Revision 5 are correctly defined in the KiCad netlist (GPIO 26 and 27 for H/V, fixing the Revision 4 error), but:

1. No Revision 5 configuration exists in `config.h`
2. No joystick port 2 code exists
3. No gamepad/joystick USB HID handling exists

To implement joystick port 2 support, you will need to:
1. Add Revision 5 configuration to `config.h`
2. Create joystick port 2 GPIO control code (similar to `quad_mouse.c`)
3. Add gamepad/joystick USB HID handling to `usb_hid.c`
4. Map USB gamepad inputs to Amiga joystick port 2 signals

