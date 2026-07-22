# Level Shifter Troubleshooting Guide

## Quick Checklist

### 1. Resistor Placement
- ✅ **5V side (TXB0108 B-side)**: Resistors must be on the **5V side** (Amiga side), NOT the 3.3V side
- ✅ **Connection**: One end to TXB0108 B-side pin, other end to **5V power supply**
- ✅ **Value**: 10kΩ to 22kΩ (10kΩ is fine, 22kΩ reduces current draw)

### 2. Which Pins Need Resistors?

**Joystick Port 1 (Revision 5)**:
- GPIO 10 (UP) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 11 (DOWN) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 12 (LEFT) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 13 (RIGHT) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 14 (FIRE) → TXB0108 B-side → **10kΩ to 5V** (if not working)

**Joystick Port 2 (Revision 5)**:
- GPIO 20 (UP) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 21 (DOWN) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 22 (LEFT) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 23 (RIGHT) → TXB0108 B-side → **10kΩ to 5V**
- GPIO 26 (FIRE) → TXB0108 B-side → **10kΩ to 5V** (if not working)

### 3. Common Issues

#### Issue: Resistors on Wrong Side
**Symptom**: Still doesn't work after adding resistors
**Solution**: Verify resistors are on **5V side (B-side)**, not 3.3V side (A-side)

#### Issue: Resistors Not Connected to 5V
**Symptom**: Still doesn't work
**Solution**: Verify one end of resistor goes to **5V power supply**, not 3.3V

#### Issue: Wrong Resistor Value
**Symptom**: Works but draws too much current
**Solution**: Use 22kΩ instead of 10kΩ to reduce current draw

#### Issue: TXB0108 OE Pin Not Enabled
**Symptom**: Nothing works
**Solution**: Verify OE (Output Enable) pin is tied to **3.3V** (VCCA)

#### Issue: Ground Connections
**Symptom**: Unreliable operation
**Solution**: Verify all grounds are connected:
- Pico GND
- TXB0108 GND (3.3V side)
- TXB0108 GND (5V side)
- Amiga GND

### 4. Testing Procedure

1. **Test Fire Button First**: If fire button works, the level shifter is working for that signal
2. **Add Resistors One at a Time**: Start with one direction (e.g., UP), test, then add others
3. **Verify Connections**: Use multimeter to verify:
   - Resistor connects TXB0108 B-side pin to 5V
   - TXB0108 A-side pin connects to Pico GPIO
   - TXB0108 B-side pin connects to Amiga

### 5. Verification Steps

1. **Power Off**: Disconnect power
2. **Check Resistor Placement**: 
   - Resistor should connect TXB0108 **B-side pin** (5V side) to **5V power**
   - NOT on A-side (3.3V side)
3. **Check Connections**:
   - One end of resistor → TXB0108 B-side pin
   - Other end of resistor → 5V power supply
4. **Power On**: Reconnect power and test

### 6. Expected Behavior

**With Pull-ups Correctly Installed**:
- ✅ All directions work independently
- ✅ Fire button works
- ✅ No delay when pressing directions
- ✅ Directions work without pressing fire button

**Without Pull-ups (or Wrong Side)**:
- ❌ Only fire button works (or nothing works)
- ❌ Directions only work when fire button is pressed
- ❌ Delayed input detection
- ❌ Inconsistent behavior

### 7. TXB0108 Pinout Reference

```
TXB0108 (8-bit bidirectional level shifter)

A-side (3.3V)          B-side (5V)
--------              --------
A1  →  Pico GPIO      B1  →  Amiga Signal
A2  →  Pico GPIO      B2  →  Amiga Signal
A3  →  Pico GPIO      B3  →  Amiga Signal
A4  →  Pico GPIO      B4  →  Amiga Signal
A5  →  Pico GPIO      B5  →  Amiga Signal
A6  →  Pico GPIO      B6  →  Amiga Signal
A7  →  Pico GPIO      B7  →  Amiga Signal
A8  →  Pico GPIO      B8  →  Amiga Signal

VCCA (3.3V)           VCCB (5V)
GNDA (3.3V GND)       GNDB (5V GND)
OE (Output Enable) → 3.3V (tied to VCCA)
```

**Pull-up resistors go on B-side (5V side)**: Connect between each B1-B8 pin and VCCB (5V).

