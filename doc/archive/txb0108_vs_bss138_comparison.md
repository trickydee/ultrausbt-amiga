# TXB0108 vs BSS138 Level Shifter Design Comparison

## Summary

**BSS138 MOSFET Design**: ✅ **Working** (proven, used in original amigahid-pico)  
**TXB0108 IC Design**: ❌ **Not working** (despite following datasheet requirements)

---

## TXB0108 Design (Not Working)

### Components
- **1 IC**: TXB0108 (8-bit bidirectional level shifter)
- **8+ resistors**: 10kΩ pull-ups on 5V side (B-side)
- **1 enable pin**: OE tied to 3.3V

### Circuit Topology
```
Pico GPIO (3.3V) ←-- TXB0108 A-side (3.3V) ←-- TXB0108 B-side (5V) --→ Amiga (5V)
                         ↑                              ↑
                    [Internal]                    [10kΩ to 5V]
```

### Characteristics
- **Automatic direction detection**: TXB0108 detects which side is driving
- **Requires pull-ups on BOTH sides**: Per datasheet (10kΩ to 50kΩ)
- **Single IC**: Simpler board layout, fewer components
- **Propagation delay**: ~5-20ns (very fast)
- **Voltage translation**: 3.3V ↔ 5V bidirectional

### Issues Encountered
- ❌ Directions not working even with pull-ups on 5V side
- ❌ Only fire button worked (GPIO 26)
- ❌ Tried: Pull-ups on 5V side only → didn't work
- ❌ Tried: Pull-ups on both sides (5V + 3.3V internal) → still didn't work
- ❌ Possible causes:
  - TXB0108 direction detection not working correctly
  - Timing issues with rapid GPIO changes
  - TXB0108 may not be suitable for this active-low, rapidly-changing signal application

### Software Configuration
- INPUT mode with pull-up enabled (3.3V side)
- Pull-ups on 5V side (external 10kΩ resistors)

---

## BSS138 MOSFET Design (Working)

### Components
- **17 MOSFETs**: BSS138 N-channel MOSFETs (one per signal)
- **34 resistors**: 10kΩ pull-ups (2 per signal: one to 5V, one to 3.3V)
- **No enable pins**: Always active

### Circuit Topology
```
Pico GPIO (3.3V) ←--[10kΩ to 3.3V]--← BSS138 Source (S)
                                              |
                                              | Gate (G) → 3.3V
                                              |
BSS138 Drain (D) --→ [10kΩ to 5V] --→ Amiga (5V)
```

### Characteristics
- **Manual direction control**: MOSFET state controlled by gate voltage
- **Requires pull-ups on BOTH sides**: 10kΩ to 5V and 10kΩ to 3.3V
- **Discrete components**: More board space, more components to place
- **Propagation delay**: ~10-50ns (slightly slower, but negligible for joystick signals)
- **Voltage translation**: 3.3V ↔ 5V bidirectional via MOSFET switching

### How It Works
1. **Pico drives LOW (3.3V side)**:
   - GPIO set to OUTPUT, drives LOW
   - MOSFET gate at 3.3V, source pulled LOW → MOSFET turns ON
   - Drain (5V side) pulled LOW → Amiga sees LOW (active)

2. **Pico drives HIGH (3.3V side)**:
   - GPIO set to INPUT with pull-up (3.3V)
   - MOSFET gate at 3.3V, source at 3.3V → MOSFET turns OFF
   - Drain (5V side) pulled HIGH by 10kΩ resistor to 5V → Amiga sees HIGH (inactive)

3. **Amiga drives LOW (5V side)**:
   - Body diode in MOSFET conducts → pulls source (3.3V side) LOW
   - Pico GPIO (INPUT) sees LOW

4. **Amiga drives HIGH (5V side)**:
   - MOSFET OFF, 3.3V side pulled HIGH by 10kΩ resistor to 3.3V
   - Pico GPIO (INPUT) sees HIGH

### Software Configuration
- INPUT mode with pull-up enabled (3.3V side) when inactive
- OUTPUT mode when active (drives LOW)
- Pull-ups on 5V side (external 10kΩ resistors)

---

## Key Differences

| Aspect | TXB0108 | BSS138 |
|--------|---------|--------|
| **Component Count** | 1 IC | 17 MOSFETs + 34 resistors |
| **Board Space** | Minimal | More space required |
| **Cost** | Lower (single IC) | Higher (more components) |
| **Complexity** | Simpler layout | More routing |
| **Direction Detection** | Automatic (internal) | Manual (gate control) |
| **Pull-up Requirements** | Both sides (per datasheet) | Both sides (proven design) |
| **Reliability** | ❌ Not working for this application | ✅ Proven, working |
| **Speed** | Faster (5-20ns) | Slightly slower (10-50ns) |
| **Suitability** | May not work well with active-low, rapidly-changing signals | ✅ Works perfectly |

---

## Why BSS138 Works Better

1. **Explicit Control**: BSS138 MOSFETs are explicitly controlled by gate voltage - no automatic direction detection that can fail
2. **Proven Design**: The original amigahid-pico uses this design and it works reliably
3. **Active-Low Signals**: BSS138 design handles active-low signals (LOW = active) more reliably
4. **Rapid GPIO Changes**: Joystick signals change rapidly - BSS138 handles this better than TXB0108's automatic detection
5. **No Direction Detection Issues**: TXB0108's automatic direction detection may fail with rapidly changing signals or specific timing patterns

---

## Recommendation

**Use BSS138 MOSFET Design** for this application:
- ✅ Proven to work
- ✅ Reliable with active-low signals
- ✅ Handles rapid GPIO changes well
- ✅ Matches original amigahid-pico design

The TXB0108, while simpler in theory, doesn't work reliably for this specific application (active-low joystick signals with rapid state changes).

---

## Software Note

The current firmware (v1.0.15) is already configured correctly for the BSS138 design:
- Pull-ups enabled on Pico side (3.3V) when inactive
- OUTPUT mode when active (drives LOW)
- Works with external pull-ups on 5V side

No software changes needed - just hardware (BSS138 MOSFETs + resistors).

