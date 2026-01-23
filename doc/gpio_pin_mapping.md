# GPIO Pin to Function Mapping

This document provides a comprehensive mapping of GPIO pins to their functions, with detailed information about joystick port directions and D-sub pin numbers.

**Board Revision:** 5 (HIDPICO_REVISION=5)  
**Note:** All pin designations are GPIO PIN NUMBERS, NOT PHYSICAL PIN NUMBERS.

---

## Joystick Port 1 (DB-9 Connector)

Joystick Port 1 shares the same connector and GPIO pins as the mouse interface. The port can operate in either **mouse mode** (quadrature encoding) or **joystick mode** (digital directions).

### DB-9 Pin to GPIO Mapping

| DB-9 Pin | Signal Name | Direction (Joystick Mode) | GPIO Pin | GPIO Signal | Active State | Description |
|----------|-------------|---------------------------|----------|-------------|-------------|-------------|
| 1 | V | **UP** | 10 | `QM1_AMIGA_V` | LOW (0) | Vertical / Up direction |
| 2 | H | **DOWN** | 9 | `QM1_AMIGA_H` | LOW (0) | Horizontal / Down direction |
| 3 | VQ | **LEFT** | 8 | `QM1_AMIGA_VQ` | LOW (0) | Vertical Quadrature / Left direction |
| 4 | HQ | **RIGHT** | 7 | `QM1_AMIGA_HQ` | LOW (0) | Horizontal Quadrature / Right direction |
| 5 | b3 | Button 3 | 13 | `QM1_AMIGA_B3` | LOW (0) | Button 3 |
| 6 | b1 | Button 1 (Fire) | 11 | `QM1_AMIGA_B1` | LOW (0) | Fire button |
| 7 | +5V | Power | - | - | N/A | Power (not connected to GPIO) |
| 8 | GND | Ground | - | - | N/A | Ground (not connected to GPIO) |
| 9 | b2 | Button 2 | 12 | `QM1_AMIGA_B2` | LOW (0) | Button 2 |

### Joystick Port 1 Direction Logic

In **joystick mode**, each direction uses a separate GPIO pin:

- **UP**: GPIO 10 (`QM1_AMIGA_V`) - DB-9 Pin 1
- **DOWN**: GPIO 9 (`QM1_AMIGA_H`) - DB-9 Pin 2
- **LEFT**: GPIO 8 (`QM1_AMIGA_VQ`) - DB-9 Pin 3
- **RIGHT**: GPIO 7 (`QM1_AMIGA_HQ`) - DB-9 Pin 4

**Note:** All signals are **active low** (LOW = active/pressed, HIGH = inactive/released).

---

## Joystick Port 2 (DB-9 Connector)

Joystick Port 2 is a dedicated joystick port (not shared with mouse). Each direction uses a separate GPIO pin.

### DB-9 Pin to GPIO Mapping

| DB-9 Pin | Signal Name | Direction | GPIO Pin | GPIO Signal | RP2040 Physical Pin | Active State | Description |
|----------|-------------|-----------|----------|-------------|---------------------|-------------|-------------|
| 1 | V | **UP** | 27 | `QM2_AMIGA_V` | 32 (GPIO27_ADC1) | LOW (0) | Vertical / Up direction |
| 2 | H | **DOWN** | 26 | `QM2_AMIGA_H` | 31 (GPIO26_ADC0) | LOW (0) | Horizontal / Down direction |
| 3 | VQ | **LEFT** | 22 | `QM2_AMIGA_VQ` | 29 | LOW (0) | Vertical Quadrature / Left direction |
| 4 | HQ | **RIGHT** | 21 | `QM2_AMIGA_HQ` | 27 | LOW (0) | Horizontal Quadrature / Right direction |
| 5 | b3 | Button 3 | 18 | `QM2_AMIGA_B3` | 24 | LOW (0) | Button 3 |
| 6 | b1 | Button 1 (Fire) | 20 | `QM2_AMIGA_B1` | 26 | LOW (0) | Fire button |
| 7 | +5V | Power | - | - | - | N/A | Power (not connected to GPIO) |
| 8 | GND | Ground | - | - | - | N/A | Ground (not connected to GPIO) |
| 9 | b2 | Button 2 | 19 | `QM2_AMIGA_B2` | 25 | LOW (0) | Button 2 |

### Joystick Port 2 Direction Logic

Each direction uses a separate GPIO pin (matches the table above):

- **UP**: GPIO 27 (`QM2_AMIGA_V`) - DB-9 Pin 1 - RP2040 Physical Pin 32
- **DOWN**: GPIO 26 (`QM2_AMIGA_H`) - DB-9 Pin 2 - RP2040 Physical Pin 31
- **LEFT**: GPIO 22 (`QM2_AMIGA_VQ`) - DB-9 Pin 3 - RP2040 Physical Pin 29
- **RIGHT**: GPIO 21 (`QM2_AMIGA_HQ`) - DB-9 Pin 4 - RP2040 Physical Pin 27

**Note:** All signals are **active low** (LOW = active/pressed, HIGH = inactive/released).

**Revision 5 Fix:** In Revision 4, the horizontal/down signal was erroneously connected to physical pin 30 (RUN). This was corrected in Revision 5, moving it to physical pin 31 (GPIO 26).

---

## Complete GPIO Pin Assignment (Revision 5)

| GPIO Pin | Function | Signal Name | DB-9 Pin (if applicable) | Direction (if applicable) | Active State |
|----------|----------|-------------|---------------------------|---------------------------|--------------|
| 2 | I2C SDA | `I2C_PIN_SDA` | - | - | - |
| 3 | I2C SCL | `I2C_PIN_SCL` | - | - | - |
| 4 | Keyboard Reset | `KBD_AMIGA_RST` | - | - | LOW |
| 5 | Keyboard Data | `KBD_AMIGA_DAT` | - | - | LOW |
| 6 | Keyboard Clock | `KBD_AMIGA_CLK` | - | - | LOW |
| 7 | Port 1 HQ / RIGHT | `QM1_AMIGA_HQ` | Port 1 Pin 4 | RIGHT | LOW |
| 8 | Port 1 VQ / LEFT | `QM1_AMIGA_VQ` | Port 1 Pin 3 | LEFT | LOW |
| 9 | Port 1 H / DOWN | `QM1_AMIGA_H` | Port 1 Pin 2 | DOWN | LOW |
| 10 | Port 1 V / UP | `QM1_AMIGA_V` | Port 1 Pin 1 | UP | LOW |
| 11 | Port 1 Button 1 (Fire) | `QM1_AMIGA_B1` | Port 1 Pin 6 | - | LOW |
| 12 | Port 1 Button 2 | `QM1_AMIGA_B2` | Port 1 Pin 9 | - | LOW |
| 13 | Port 1 Button 3 | `QM1_AMIGA_B3` | Port 1 Pin 5 | - | LOW |
| 14 | Unused | - | - | - | - |
| 15 | Unused | - | - | - | - |
| 16 | Unused | - | - | - | - |
| 17 | Unused | - | - | - | - |
| 18 | Port 2 Button 3 | `QM2_AMIGA_B3` | Port 2 Pin 5 | - | LOW |
| 19 | Port 2 Button 2 | `QM2_AMIGA_B2` | Port 2 Pin 9 | - | LOW |
| 20 | Port 2 Button 1 (Fire) | `QM2_AMIGA_B1` | Port 2 Pin 6 | - | LOW |
| 21 | Port 2 HQ / RIGHT | `QM2_AMIGA_HQ` | Port 2 Pin 4 | RIGHT | LOW |
| 22 | Port 2 VQ / LEFT | `QM2_AMIGA_VQ` | Port 2 Pin 3 | LEFT | LOW |
| 23 | Unused | - | - | - | - |
| 24 | Unused | - | - | - | - |
| 25 | LED | Onboard LED | - | - | - |
| 26 | Port 2 H / DOWN | `QM2_AMIGA_H` | Port 2 Pin 2 | DOWN | LOW |
| 27 | Port 2 V / UP | `QM2_AMIGA_V` | Port 2 Pin 1 | UP | LOW |
| 28 | Unused | - | - | - | - |

---

## Standard Amiga DB-9 Joystick Port Pinout

For reference, the standard Amiga joystick port pinout:

| DB-9 Pin | Signal | Description | Typical Usage |
|----------|--------|-------------|---------------|
| 1 | Up | Vertical up direction | Joystick up |
| 2 | Down | Vertical down direction | Joystick down |
| 3 | Left | Horizontal left direction | Joystick left |
| 4 | Right | Horizontal right direction | Joystick right |
| 5 | Fire | Fire button | Primary action button |
| 6 | +5V | Power supply | Not used by joystick |
| 7 | Button 2 | Second button | Secondary action button |
| 8 | Ground | Ground reference | Common ground |
| 9 | Button 3 | Third button | Tertiary action button |

**Note:** The Amiga uses quadrature encoding for mouse movement, which requires the HQ and VQ signals (pins 3 and 4) in addition to the basic H and V signals (pins 1 and 2).

---

## Signal Logic Summary

### Active Low Signals

All GPIO signals for joystick ports are **active low**:
- **Direction pins**: LOW (0) = direction active/pressed, HIGH (1) = direction inactive/released
- **Button pins**: LOW (0) = button pressed, HIGH (1) = button not pressed

### Direction Signal Behavior

- When a direction is **active**: The corresponding GPIO pin is set to LOW (0)
- When a direction is **inactive**: The corresponding GPIO pin is set to HIGH (1) or configured as input (pulled high)
- Multiple directions can be active simultaneously (e.g., UP + RIGHT for diagonal)

### Button Signal Behavior

- When a button is **pressed**: The corresponding GPIO pin is set to LOW (0)
- When a button is **released**: The corresponding GPIO pin is set to HIGH (1) or configured as input (pulled high)

---

## Code References

- GPIO definitions: `src/config.h` (QM1_AMIGA_* and QM2_AMIGA_* macros)
- Joystick Port 1 implementation: `src/platform/amiga/joystick_port1.c`
- Joystick Port 2 implementation: `src/platform/amiga/joystick_port2.c`
- Mouse implementation: `src/platform/amiga/quad_mouse.c`
- Keyboard implementation: `src/platform/amiga/keyboard_serial_io.c`

---

## Revision History

- **Revision 4**: Initial implementation. Port 2 had an error where horizontal/down was connected to physical pin 30 (RUN).
- **Revision 5**: Fixed Port 2 horizontal/down to use physical pin 31 (GPIO 26) and vertical/up to use physical pin 32 (GPIO 27).

---

## Notes

1. **Port 1 Sharing**: Joystick Port 1 shares GPIO pins with the mouse interface. The firmware switches between mouse mode (quadrature encoding) and joystick mode (digital directions) based on user configuration.

2. **Port 2 Dedicated**: Joystick Port 2 is dedicated to joystick use only and does not support mouse mode.

3. **Physical Pin Numbers**: The physical pin numbers refer to the RP2040/RP2350 physical package pins, not the GPIO numbers. GPIO 26 corresponds to physical pin 31, and GPIO 27 corresponds to physical pin 32.

4. **ADC Pins**: GPIO 26 and GPIO 27 are also ADC-capable pins (ADC0 and ADC1), but are used as digital GPIO in this application.

