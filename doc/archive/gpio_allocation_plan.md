# GPIO Allocation Plan for Atari/Amiga Dual Compatibility

This document outlines GPIO pin allocation to maintain compatibility with both Atari and Amiga firmware while adding the remaining joystick buttons.

## Current GPIO Usage

### Amiga Firmware (Revision 5)
- **I2C Display**: GPIO 8, 9 (i2c0)
- **Keyboard**: GPIO 4, 5, 6 (RST, DAT, CLK)
- **Port 1 Directions**: GPIO 10, 11, 12, 13 (UP, DOWN, LEFT, RIGHT)
- **Port 1 Fire**: GPIO 14
- **Port 1 Buttons 2/3**: GPIO 12, 13 ⚠️ **CONFLICTS** with LEFT/RIGHT directions
- **Port 2 Directions**: GPIO 19, 20, 21, 22 (UP, DOWN, LEFT, RIGHT)
- **Port 2 Fire**: GPIO 26
- **Port 2 Buttons 2/3**: GPIO 19, 18 ⚠️ **CONFLICTS** with UP direction

### Atari Firmware
- **I2C Display**: GPIO 8, 9 (i2c0) ✅ **MATCHES** Amiga
- **UART**: GPIO 4, 5 (TX, RX) ⚠️ **CONFLICTS** with Amiga Keyboard (4, 5, 6)
- **UI Buttons**: GPIO 16, 17, 18 (LEFT, MIDDLE, RIGHT)
- **JOY1**: GPIO 10, 11, 12, 13, 14 ✅ **MATCHES** Amiga Port 1
- **JOY0**: GPIO 19, 20, 21, 22, 26 ✅ **MATCHES** Amiga Port 2

## Available GPIO Pins (0-23)

**Used GPIOs**: 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 18, 19, 20, 21, 22

**Free GPIOs**: 0, 1, 2, 3, 7, 15, 16, 17, 23

**Special GPIOs**:
- GPIO 24-29: Special functions (ADC, etc.) - avoid unless necessary
- GPIO 25: Onboard LED (can be used but has LED attached)

## Requirements

1. ✅ **Reserve Atari GPIOs**: 4, 5, 16, 17, 18 (UART + UI Buttons)
2. ✅ **Add Port 1 Buttons 2/3**: Need 2 GPIOs (currently conflicting with 12, 13)
3. ✅ **Add Port 2 Buttons 2/3**: Need 2 GPIOs (currently conflicting with 19, 18)
4. ✅ **Ensure Amiga Keyboard doesn't conflict**: Currently uses 4, 5, 6

## Recommended GPIO Allocation

### Option 1: Minimal Changes (Recommended)

**Port 1 Additional Buttons:**
- **Button 2**: GPIO 15 (free)
- **Button 3**: GPIO 16 (free, but conflicts with Atari UI Button LEFT)

**Port 2 Additional Buttons:**
- **Button 2**: GPIO 17 (free, but conflicts with Atari UI Button MIDDLE)
- **Button 3**: GPIO 23 (free)

**Issues:**
- GPIO 16 conflicts with Atari UI Button LEFT
- GPIO 17 conflicts with Atari UI Button MIDDLE

### Option 2: Full Compatibility (Best)

**Port 1 Additional Buttons:**
- **Button 2**: GPIO 15 (free)
- **Button 3**: GPIO 23 (free)

**Port 2 Additional Buttons:**
- **Button 2**: GPIO 0 (free)
- **Button 3**: GPIO 1 (free)

**Reserved for Atari:**
- GPIO 4, 5: UART (conflicts with Amiga Keyboard - firmware handles this)
- GPIO 16, 17, 18: UI Buttons (reserved, not used by Amiga)

**Advantages:**
- ✅ No conflicts with Atari GPIOs
- ✅ All buttons have dedicated GPIOs
- ✅ Amiga keyboard (4, 5, 6) doesn't conflict with Atari UART (4, 5) - different firmware
- ✅ Atari UI buttons (16, 17, 18) reserved but not used by Amiga

### Option 3: Alternative (If GPIO 0/1 are problematic)

**Port 1 Additional Buttons:**
- **Button 2**: GPIO 15
- **Button 3**: GPIO 23

**Port 2 Additional Buttons:**
- **Button 2**: GPIO 2
- **Button 3**: GPIO 3

**Note**: GPIO 2, 3 were used for I2C in Revision 4, but Revision 5 uses GPIO 8, 9, so 2, 3 are free.

## Recommended Solution: Option 2

### Final GPIO Allocation

#### Amiga Firmware GPIOs:
- **I2C**: 8, 9
- **Keyboard**: 4, 5, 6
- **Port 1 Directions**: 10, 11, 12, 13
- **Port 1 Fire**: 14
- **Port 1 Button 2**: 15 ⭐ **NEW**
- **Port 1 Button 3**: 23 ⭐ **NEW**
- **Port 2 Directions**: 19, 20, 21, 22
- **Port 2 Fire**: 26
- **Port 2 Button 2**: 0 ⭐ **NEW**
- **Port 2 Button 3**: 1 ⭐ **NEW**

#### Atari Firmware GPIOs (Reserved):
- **I2C**: 8, 9 ✅
- **UART**: 4, 5 ⚠️ (conflicts with Amiga Keyboard, but different firmware)
- **UI Buttons**: 16, 17, 18 ✅ (reserved, not used by Amiga)
- **JOY1**: 10, 11, 12, 13, 14 ✅
- **JOY0**: 19, 20, 21, 22, 26 ✅

#### Free/Unused GPIOs:
- GPIO 2, 3, 7 (available for future use)

## Implementation Notes

### 1. Firmware Compatibility
- **Amiga firmware**: Uses GPIO 4, 5, 6 for keyboard (UART not used)
- **Atari firmware**: Uses GPIO 4, 5 for UART (keyboard not used)
- **Solution**: Different firmware = no runtime conflict ✅

### 2. Button Remapping
Current code has conflicts that need to be resolved:
- Port 1 Button 2/3 currently use GPIO 12, 13 (conflicts with LEFT/RIGHT)
- Port 2 Button 2/3 currently use GPIO 19, 18 (conflicts with UP)

**Required changes:**
```c
// Port 1 buttons (NEW)
#  define QM1_AMIGA_B2   15  // Button 2 - NEW GPIO
#  define QM1_AMIGA_B3   23  // Button 3 - NEW GPIO

// Port 2 buttons (NEW)
#  define QM2_AMIGA_B2   0   // Button 2 - NEW GPIO
#  define QM2_AMIGA_B3   1   // Button 3 - NEW GPIO
```

### 3. Atari GPIO Reservation
To ensure compatibility, document that these GPIOs are reserved:
- GPIO 4, 5: Atari UART (uart1)
- GPIO 16, 17, 18: Atari UI Buttons
- GPIO 18: Also used by Atari UI Button RIGHT

**Note**: GPIO 18 is currently used by Amiga Port 2 Button 3, but this conflicts with Port 2 UP (GPIO 19). Moving Button 3 to GPIO 1 resolves this and frees GPIO 18 for Atari.

## GPIO Conflict Matrix

| GPIO | Amiga Usage | Atari Usage | Conflict? | Resolution |
|------|-------------|-------------|-----------|------------|
| 0 | Port 2 Button 2 | - | No | ✅ |
| 1 | Port 2 Button 3 | - | No | ✅ |
| 2 | - | - | No | Free |
| 3 | - | - | No | Free |
| 4 | Keyboard DAT | UART TX | ⚠️ | Different firmware |
| 5 | Keyboard CLK | UART RX | ⚠️ | Different firmware |
| 6 | Keyboard RST | - | No | ✅ |
| 7 | - | - | No | Free |
| 8 | I2C SDA | I2C SDA | ✅ | Shared |
| 9 | I2C SCL | I2C SCL | ✅ | Shared |
| 10 | Port 1 UP | JOY1 UP | ✅ | Shared |
| 11 | Port 1 DOWN | JOY1 DOWN | ✅ | Shared |
| 12 | Port 1 LEFT | JOY1 LEFT | ✅ | Shared |
| 13 | Port 1 RIGHT | JOY1 RIGHT | ✅ | Shared |
| 14 | Port 1 FIRE | JOY1 FIRE | ✅ | Shared |
| 15 | Port 1 Button 2 | - | No | ✅ |
| 16 | - | UI Button LEFT | ⚠️ | Reserved for Atari |
| 17 | - | UI Button MIDDLE | ⚠️ | Reserved for Atari |
| 18 | - | UI Button RIGHT | ⚠️ | Reserved for Atari |
| 19 | Port 2 UP | JOY0 UP | ✅ | Shared |
| 20 | Port 2 DOWN | JOY0 DOWN | ✅ | Shared |
| 21 | Port 2 LEFT | JOY0 LEFT | ✅ | Shared |
| 22 | Port 2 RIGHT | JOY0 RIGHT | ✅ | Shared |
| 23 | Port 1 Button 3 | - | No | ✅ |
| 24-29 | Special functions | Special functions | - | Avoid |
| 26 | Port 2 FIRE | JOY0 FIRE | ✅ | Shared |

## Summary

**Recommended GPIO Allocation:**
- **Port 1 Button 2**: GPIO 15
- **Port 1 Button 3**: GPIO 23
- **Port 2 Button 2**: GPIO 0
- **Port 2 Button 3**: GPIO 1

**Reserved for Atari:**
- GPIO 4, 5: UART (different firmware, no runtime conflict)
- GPIO 16, 17, 18: UI Buttons (not used by Amiga firmware)

**Result:**
- ✅ All joystick buttons have dedicated GPIOs
- ✅ No conflicts between Amiga and Atari firmware
- ✅ Hardware compatible with both firmware types
- ✅ Users can flash either firmware as needed

