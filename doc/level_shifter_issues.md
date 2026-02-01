# Level Shifter Issues and Solutions

## Problem Description

After adding 8-way level shifters (5V ↔ 3.3V) to protect joystick GPIOs, the following issues were observed:

1. **Delayed input detection** - Movement inputs are delayed
2. **Fire button dependency** - Movement is only detected when combined with fire button press
3. **Inconsistent behavior** - Signals don't work reliably

## Root Cause Analysis

### Current GPIO Configuration

The current code uses this approach for inactive signals:
```c
// Inactive: set to HIGH (1) by setting as input (pulled high)
gpio_set_dir(gpio, GPIO_IN);
gpio_set_pulls(gpio, true, false);  // Enable pull-up, disable pull-down
```

**Problem**: When using level shifters, the Pico's internal pull-ups (3.3V) can conflict with:
- The level shifter's direction detection
- The Amiga's pull-ups (5V) on the other side
- The level shifter's internal pull-up/pull-down configuration

### Why Fire Button Makes It Work

The fire button dependency suggests:
1. **Direction detection issue**: Bidirectional level shifters need to detect signal direction. When fire is pressed (OUTPUT LOW), it might "wake up" or configure the shifter, allowing other signals to pass through.
2. **Pull-up conflict**: The Pico's 3.3V pull-up might not be strong enough or might conflict with the level shifter's 5V side pull-ups.
3. **Enable pin**: Some level shifters have enable pins that might need a reference signal.

## Solutions

### Solution 1: INPUT Mode with NO Pull-Up (Current Approach)

When using bidirectional level shifters, set GPIO to **INPUT with NO pull-up** and let the level shifter translate the Amiga's 5V pull-up.

**Implementation**:
```c
// Inactive: Set as INPUT with NO pull-up
gpio_set_dir(gpio, GPIO_IN);
gpio_set_pulls(gpio, false, false);  // No pull-up - level shifter handles translation
```

**How it works**:
- Pico side: INPUT (high-impedance, no pull-up)
- Amiga side: 5V pull-up pulls the 5V side HIGH
- Level shifter: Detects 5V side is driving HIGH, translates to 3.3V HIGH on Pico side
- Amiga sees: HIGH (5V) = inactive ✓

**Why this should work**:
- The level shifter detects that the 5V side (Amiga) is driving HIGH via pull-up
- The level shifter translates this to 3.3V HIGH on the Pico side
- No conflict between Pico's 3.3V pull-up and level shifter direction detection
- When Pico drives LOW (OUTPUT), level shifter detects 3.3V side driving, translates to 0V on 5V side

**Note**: This requires the level shifter to properly detect direction. If this doesn't work, the level shifter may need external pull-ups or a different configuration.

### Solution 2: Add External Pull-Ups on 5V Side (REQUIRED for TXB0108)

**The TXB0108 REQUIRES pull-up resistors on BOTH sides for proper bidirectional operation.**

Add external pull-up resistors (10kΩ to 50kΩ) on the 5V side (B side) of the TXB0108 level shifter.

**Circuit**:
```
Amiga (5V) ←--[10kΩ pull-up to 5V]--← TXB0108 B side ←-- TXB0108 A side ←-- Pico GPIO (INPUT, no pull-up)
```

**Why this is critical for TXB0108**:
- TXB0108 uses automatic direction detection based on which side is driving
- Without pull-ups on the 5V side, the level shifter cannot properly detect when the Amiga's pull-up is "driving" HIGH
- The weak internal pull-ups in TXB0108 are not sufficient for reliable operation
- External 10kΩ pull-ups on the 5V side ensure proper HIGH state detection

**Implementation**:
- Add pull-up resistors from each TXB0108 B-side pin (5V side) to 5V
- **Resistor value options**:
  - **22kΩ to 47kΩ (recommended)**: Reduces current draw, still provides sufficient pull-up strength
  - **10kΩ**: Works but draws more current (effective ~5kΩ with Amiga's internal pull-up)
- Connect between level shifter B-side and 5V power supply
- These work in parallel with the Amiga's internal pull-ups (~10kΩ)

**Current Draw Consideration**:
- With 10kΩ external + 10kΩ internal = ~5kΩ effective resistance
- Current: 5V / 5kΩ = 1mA per signal (typically safe for Amiga)
- With 22kΩ external + 10kΩ internal = ~6.9kΩ effective resistance  
- Current: 5V / 6.9kΩ = ~0.7mA per signal (safer, still sufficient)
- **Recommendation**: Use 22kΩ to 47kΩ to reduce current draw while still providing adequate pull-up strength

### Solution 3: Configure Level Shifter Direction

If using bidirectional level shifters (like TXB0108, SN74LVC8T245), ensure:
- Direction pins are configured correctly
- Enable pins (if present) are tied appropriately
- VCC_A (3.3V) and VCC_B (5V) are properly connected

### Solution 4: Use Unidirectional Level Shifters

For joystick ports, signals are **always driven FROM Pico TO Amiga** (never the reverse). Consider using unidirectional level shifters configured for 3.3V → 5V direction only.

**Advantages**:
- No direction detection needed
- Lower propagation delay
- More reliable

## Recommended Implementation

### Modified GPIO Configuration for Level Shifters

Create a new function or modify `amiga_gpio_set_active_low()` to support level shifter mode:

```c
void amiga_gpio_set_active_low_with_level_shifter(uint32_t gpio, bool active)
{
    if (gpio >= MAX_GPIO) {
        return;
    }
    
    if (active) {
        // Active: Drive LOW (0V) as OUTPUT
        gpio_set_pulls(gpio, false, false);  // Disable pulls
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, 0);
        __sync_synchronize();
    } else {
        // Inactive: High-impedance INPUT with NO pull-up
        // Let level shifter and Amiga pull-ups handle the HIGH state
        gpio_set_dir(gpio, GPIO_IN);
        gpio_set_pulls(gpio, false, false);  // NO pull-up - critical for level shifters
        __sync_synchronize();
    }
}
```

### Configuration Option

Add a compile-time or runtime option to enable level shifter mode:

```c
// In config.h
#define ENABLE_LEVEL_SHIFTER 1  // Set to 1 if using level shifters

// In gpio_util.c
void amiga_gpio_set_active_low(uint32_t gpio, bool active)
{
    if (gpio >= MAX_GPIO) {
        return;
    }
    
    if (active) {
        gpio_set_pulls(gpio, false, false);
        gpio_set_dir(gpio, GPIO_OUT);
        gpio_put(gpio, 0);
        __sync_synchronize();
    } else {
        gpio_set_dir(gpio, GPIO_IN);
#if ENABLE_LEVEL_SHIFTER
        // Level shifter mode: No pull-up, rely on external/Amiga pull-ups
        gpio_set_pulls(gpio, false, false);
#else
        // Direct connection: Use Pico's internal pull-up
        gpio_set_pulls(gpio, true, false);
#endif
        __sync_synchronize();
    }
}
```

## Testing Recommendations

1. **Test with pull-ups disabled**: Modify code to disable Pico pull-ups and test
2. **Check level shifter datasheet**: Verify direction/enable pin requirements
3. **Measure signals**: Use oscilloscope to check:
   - 3.3V side signals (Pico GPIO)
   - 5V side signals (Amiga side)
   - Propagation delay
4. **Test individual signals**: Test each direction/button independently to isolate issues

## Level Shifter Selection Notes

### Recommended Level Shifters for This Application

1. **TXB0108** (8-bit bidirectional):
   - Automatic direction detection
   - **REQUIRES pull-up resistors on BOTH sides (A and B) for proper operation**
   - OE (Output Enable) pin - typically tied to VCCA (3.3V) to enable
   - **Critical**: Without pull-ups on the 5V side (B side), direction detection may fail
   - Pull-up values: 10kΩ to 50kΩ recommended
   - The TXB0108 uses weak internal pull-ups, but external pull-ups improve reliability

2. **SN74LVC8T245** (8-bit bidirectional):
   - Direction pin (DIR) must be set
   - Enable pin (OE) must be controlled
   - Faster than TXB0108

3. **Unidirectional (3.3V → 5V only)**:
   - Simpler, more reliable for this use case
   - Lower cost
   - No direction detection needed

## Additional Considerations

### Propagation Delay

Level shifters add propagation delay (typically 5-20ns). This should be negligible for joystick signals, but if you're seeing delays, check:
- Level shifter speed rating
- Capacitive loading on signals
- Power supply stability

### Power Sequencing

Ensure proper power sequencing:
- 3.3V and 5V should be stable before signals are applied
- Level shifter should be powered before Pico starts driving signals

### Ground Connection

**Critical**: Ensure a common ground between:
- Pico GND
- Level shifter GND (3.3V side)
- Level shifter GND (5V side)  
- Amiga GND

All grounds must be connected for level shifters to work correctly.

## Additional Troubleshooting

### If Only Fire Button Works (TXB0108 Specific)

If only the fire button works but directions don't, this is **almost certainly due to missing pull-up resistors on the 5V side** of the TXB0108:

1. **TXB0108 requires external pull-ups**: The TXB0108 datasheet explicitly requires 10kΩ to 50kΩ pull-up resistors on BOTH sides for proper operation.
2. **Fire button works because**: It might have different timing or be driven more consistently, allowing the weak internal pull-ups to function temporarily.
3. **Directions fail because**: Without proper pull-ups on the 5V side, the level shifter cannot detect when the Amiga's pull-up is driving HIGH, causing direction detection to fail.
4. **Solution**: **Add 10kΩ pull-up resistors from each TXB0108 B-side pin (5V side) to 5V power supply.**

### Potential Solutions to Try

1. **Add external pull-ups on 5V side**: If the level shifter doesn't have internal pull-ups, add 10kΩ resistors from each 5V signal to 5V
2. **Check level shifter enable pins**: Some level shifters have enable pins that must be tied correctly
3. **Verify level shifter type**: Ensure you're using a bidirectional level shifter (not unidirectional)
4. **Check signal timing**: Add small delays between direction changes to allow level shifter to settle
5. **Try unidirectional level shifters**: Since signals only go FROM Pico TO Amiga, unidirectional shifters might work better

### Questions to Answer

1. **What level shifter chip are you using?** (TXB0108, SN74LVC8T245, etc.)
2. **Are enable/direction pins configured correctly?**
3. **Do you have pull-up resistors on the 5V side?**
4. **Are all grounds connected?**
5. **What happens if you test with a multimeter/scope?**

## Next Steps

### IMMEDIATE ACTION REQUIRED (TXB0108)

**You MUST add external pull-up resistors on the 5V side of the TXB0108 for proper operation.**

1. **Add 10kΩ pull-up resistors**: Connect a 10kΩ resistor from each TXB0108 B-side pin (5V side) to 5V power supply.
   - One resistor per signal line (UP, DOWN, LEFT, RIGHT, FIRE, etc.)
   - Connect between the TXB0108 B-side pin and 5V
   - These work in parallel with the Amiga's internal pull-ups (~10kΩ)

2. **Current software configuration**: INPUT mode with NO pull-up on Pico side (already correct)
   - This lets the level shifter translate the Amiga's 5V pull-up
   - With external pull-ups on 5V side, the TXB0108 will properly detect direction

3. **Verify hardware**: After adding pull-ups, verify:
   - All grounds are connected (Pico, level shifter 3.3V GND, level shifter 5V GND, Amiga)
   - OE pin is tied to 3.3V (VCCA) to enable the level shifter
   - Power supplies are stable (3.3V and 5V)

4. **Test incrementally**: After adding pull-ups, test one signal at a time to verify operation

5. **Measure signals**: Use scope/multimeter to verify:
   - 3.3V side signals (Pico GPIO) are correct
   - 5V side signals (Amiga side) are correct
   - Level shifter is translating properly in both directions

### Why This Is Required

The TXB0108 datasheet (Section 8.2.1) explicitly states:
> "For proper operation, pull-up resistors must be connected to all I/O pins on both sides of the device."

Without these pull-ups, the automatic direction detection feature of the TXB0108 will not work reliably, which explains why only the fire button works (it might work intermittently due to timing).

