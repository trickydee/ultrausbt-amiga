# GPIO Pin Assignments

This document details the GPIO pin assignments for the amigahid-pico project.

**IMPORTANT:** All pin designations are GPIO PIN NUMBERS, NOT PHYSICAL PIN NUMBERS.

## Board Revision

The default board revision is **Revision 4** (`HIDPICO_REVISION=4`). Pin assignments differ between revisions.

---

## Keyboard Interface (Revision 4)

The keyboard interface uses **3 GPIO pins** for serial communication with the Amiga:

| Function | GPIO Pin | Signal Name | Description |
|----------|----------|-------------|-------------|
| Reset | 4 | `KBD_AMIGA_RST` | Reset signal (active low) |
| Data | 5 | `KBD_AMIGA_DAT` | Serial data line |
| Clock | 6 | `KBD_AMIGA_CLK` | Serial clock line |

### Notes:
- All signals are **active low**
- The keyboard interface uses bit-banged serial communication
- Reset is not required for big-box Amigas (A2000, A3000, A4000), which use clock to signal reset

### Revision 2 Differences:
- Reset: GPIO 10
- Data: GPIO 11
- Clock: GPIO 12

---

## Mouse Interface (Revision 4)

The mouse interface uses **7 GPIO pins** for quadrature-encoded mouse signals:

| Function | GPIO Pin | Signal Name | Description |
|----------|----------|-------------|-------------|
| Horizontal Quadrature | 7 | `QM1_AMIGA_HQ` | Horizontal quadrature signal |
| Vertical Quadrature | 8 | `QM1_AMIGA_VQ` | Vertical quadrature signal |
| Horizontal | 9 | `QM1_AMIGA_H` | Horizontal direction signal |
| Vertical | 10 | `QM1_AMIGA_V` | Vertical direction signal |
| Left Button | 11 | `QM1_AMIGA_B1` | Left mouse button (active low) |
| Right Button | 12 | `QM1_AMIGA_B2` | Right mouse button (active low) |
| Middle Button | 13 | `QM1_AMIGA_B3` | Middle mouse button (active low) |

### Notes:
- All signals are **active low** (logic 0 = active, logic 1 = inactive)
- Uses **quadrature encoding** for motion detection (H/HQ and V/VQ pairs)
- Mouse motion processing runs on **Core 1** (separate from keyboard handling on Core 0)
- The mouse shares the same connector as **Joystick Port 1** on the Amiga

### Revision 2 Differences:
- HQ: GPIO 7 (same)
- VQ: GPIO 6
- H: GPIO 9 (same)
- V: GPIO 8
- B1: GPIO 22
- B2: GPIO 26
- B3: GPIO 27

---

## Joystick Port 1 (Revision 4)

Joystick Port 1 uses the **same GPIO pins as the mouse interface** (they share the same connector on the Amiga):

| Function | GPIO Pin | Signal Name | Description |
|----------|----------|-------------|-------------|
| Horizontal Quadrature | 7 | `/lcont1 hq` | Horizontal quadrature signal |
| Vertical Quadrature | 8 | `/lcont1 vq` | Vertical quadrature signal |
| Horizontal | 9 | `/lcont1 h` | Horizontal direction signal |
| Vertical | 10 | `/lcont1 v` | Vertical direction signal |
| Button 1 (Fire) | 11 | `/lcont1 b1` | Fire button (active low) |
| Button 2 | 12 | `/lcont1 b2` | Second button (active low) |
| Button 3 | 13 | `/lcont1 b3` | Third button (active low) |

### Notes:
- All signals are **active low**
- This port shares the same connector as the mouse on the Amiga
- The same GPIO pins are used for both mouse and joystick port 1

---

## Joystick Port 2 (Revision 4)

Joystick Port 2 uses **7 GPIO pins**:

| Function | GPIO Pin | Signal Name | Description | Notes |
|----------|----------|-------------|-------------|-------|
| Button 1 (Fire) | 18 | `/lcont2 b1` | Fire button (active low) | |
| Button 2 | 19 | `/lcont2 b2` | Second button (active low) | |
| Button 3 | 20 | `/lcont2 b3` | Third button (active low) | |
| Horizontal Quadrature | 21 | `/lcont2 hq` | Horizontal quadrature signal | |
| Vertical Quadrature | 22 | `/lcont2 vq` | Vertical quadrature signal | |
| Horizontal | 26 | `/lcont2 h` | Horizontal direction signal | GPIO26_ADC0 |
| Vertical | 27 | `/lcont2 v` | Vertical direction signal | GPIO27_ADC1 |

### Notes:
- All signals are **active low**
- **WARNING:** According to the errata, there was an issue in Revision 4 where physical pin 30 (RUN, not a GPIO) was erroneously attached as the horizontal/down line. This was fixed in Revision 5, moving h/down to physical pin 31 (GPIO 26) and v/up to physical pin 32 (GPIO 27).
- **STATUS:** Joystick Port 2 may not be fully implemented in the current codebase and may need fixes. The README notes that "the keyboard and controller port 1 are correct at time of writing, and will be fixed for the second controller port when the next prototype arrives."

---

## I2C Display Interface (Revision 4)

The I2C display (SSD1306 OLED) uses **2 GPIO pins**:

| Function | GPIO Pin | Signal Name | Description |
|----------|----------|-------------|-------------|
| SDA | 2 | `I2C_PIN_SDA` | I2C data line |
| SCL | 3 | `I2C_PIN_SCL` | I2C clock line |

### Notes:
- Uses **I2C Port 1** (`i2c1`)
- I2C IRQ number: 24
- The display is optional and can be disabled for performance

### Revision 2 Differences:
- Uses I2C Port 0 (`i2c0`)
- SDA: GPIO 4
- SCL: GPIO 5
- I2C IRQ number: 23

---

## Summary Table (Revision 4)

| GPIO | Function | Signal Name |
|------|----------|-------------|
| 2 | I2C SDA | Display data |
| 3 | I2C SCL | Display clock |
| 4 | Keyboard Reset | KBD_AMIGA_RST |
| 5 | Keyboard Data | KBD_AMIGA_DAT |
| 6 | Keyboard Clock | KBD_AMIGA_CLK |
| 7 | Mouse/Port1 HQ | QM1_AMIGA_HQ / /lcont1 hq |
| 8 | Mouse/Port1 VQ | QM1_AMIGA_VQ / /lcont1 vq |
| 9 | Mouse/Port1 H | QM1_AMIGA_H / /lcont1 h |
| 10 | Mouse/Port1 V | QM1_AMIGA_V / /lcont1 v |
| 11 | Mouse/Port1 B1 | QM1_AMIGA_B1 / /lcont1 b1 |
| 12 | Mouse/Port1 B2 | QM1_AMIGA_B2 / /lcont1 b2 |
| 13 | Mouse/Port1 B3 | QM1_AMIGA_B3 / /lcont1 b3 |
| 14 | Unused | - |
| 15 | Unused | - |
| 16 | Unused | - |
| 17 | Unused | - |
| 18 | Port2 B3 | /lcont2 b3 |
| 19 | Port2 B2 | /lcont2 b2 |
| 20 | Port2 B1 | /lcont2 b1 |
| 21 | Port2 HQ | /lcont2 hq |
| 22 | Port2 VQ | /lcont2 vq |
| 23 | Unused | - |
| 24 | Unused | - |
| 25 | LED | Onboard LED (PICO_DEFAULT_LED_PIN) |
| 26 | Port2 H | /lcont2 h (GPIO26_ADC0) |
| 27 | Port2 V | /lcont2 v (GPIO27_ADC1) |
| 28 | Unused | - |

---

## Amiga Joystick Port Pinout (DB-9 Connector)

For reference, the standard Amiga joystick port pinout:

| DB-9 Pin | Signal | Description |
|----------|--------|-------------|
| 1 | Up | Vertical up direction |
| 2 | Down | Vertical down direction |
| 3 | Left | Horizontal left direction |
| 4 | Right | Horizontal right direction |
| 5 | Fire | Fire button |
| 6 | +5V | Power (not used by joystick) |
| 7 | Button 2 | Second button (if supported) |
| 8 | Ground | Ground |
| 9 | Button 3 | Third button (if supported) |

**Note:** The Amiga uses quadrature encoding for mouse movement, which requires the HQ and VQ signals in addition to the basic H and V signals.

---

## Known Issues

1. **Joystick Port 2:** According to the errata and README, Joystick Port 2 may not be fully working in the current codebase. The errata mentions that in Revision 4, pin 30 (RUN) was erroneously used for the horizontal/down line on Port 2, which was corrected in Revision 5.

2. **Revision 5:** The README notes that "the keyboard and controller port 1 are correct at time of writing, and will be fixed for the second controller port when the next prototype arrives."

---

## References

- `src/config.h` - GPIO pin definitions
- `src/platform/amiga/keyboard_serial_io.c` - Keyboard implementation
- `src/platform/amiga/quad_mouse.c` - Mouse implementation
- `doc/errata.md` - Known hardware issues
- `doc/hardware.md` - Hardware connection guide

