# Original Level Shifter Circuit Analysis

## Summary

The original amigahid-pico design uses **BSS138 N-channel MOSFETs** for level shifting, not TXB0108 ICs. The circuit includes **pull-up resistors on BOTH sides** (5V and 3.3V) for each signal.

## Circuit Components

### MOSFETs
- **17 BSS138 N-channel MOSFETs** (Q1-Q17)
- One MOSFET per signal line (keyboard, joystick, mouse)

### Resistors
- **34 resistors total** (R1-R34)
- **2 resistors per signal**: One to 5V, one to 3.3V
- All resistors are **R1002** (10kΩ, 0603 SMD)

## Circuit Topology

Each signal uses a **BSS138 MOSFET-based level shifter** with the following configuration:

```
Pico GPIO (3.3V) ←--[R2: 10kΩ to 3.3V]--← BSS138 Source (S)
                                              |
                                              | Gate (G) → 3.3V
                                              |
BSS138 Drain (D) --→ [R1: 10kΩ to 5V] --→ Amiga (5V)
```

### Example: Keyboard Clock Signal

From the netlist:
- **Q1 (BSS138)**:
  - Gate (pin 1): Connected to **+3V3**
  - Source (pin 2): Connected to **R2** (10kΩ to 3.3V) and **GPIO6** (`/lkbclock`)
  - Drain (pin 3): Connected to **R1** (10kΩ to 5V) and **Amiga keyboard clock** (`/kbclock`)

### Example: Joystick Port 1 Fire Button

- **Q8 (BSS138)**:
  - Gate (pin 1): Connected to **+3V3**
  - Source (pin 2): Connected to **R16** (10kΩ to 3.3V) and **GPIO11** (`/lcont1 b1`)
  - Drain (pin 3): Connected to **R15** (10kΩ to 5V) and **Amiga joystick port 1 button 1** (`/cont1 b1`)

## Key Observations

1. **Pull-up resistors on BOTH sides**: Each signal has:
   - One 10kΩ resistor to **5V** (on the Amiga side)
   - One 10kΩ resistor to **3.3V** (on the Pico side)

2. **BSS138 MOSFET configuration**:
   - Gate tied to 3.3V (enables bidirectional operation)
   - Source on 3.3V side (Pico)
   - Drain on 5V side (Amiga)

3. **Bidirectional operation**: The BSS138 MOSFET acts as a bidirectional level shifter:
   - When Pico drives LOW (3.3V side): MOSFET turns on, pulls 5V side LOW
   - When Pico is HIGH (3.3V side): MOSFET is off, 5V side pulled HIGH by R1 (10kΩ to 5V)
   - When Amiga drives LOW (5V side): Body diode conducts, pulls 3.3V side LOW
   - When Amiga is HIGH (5V side): 3.3V side pulled HIGH by R2 (10kΩ to 3.3V)

## Comparison with TXB0108

### Original Design (BSS138 MOSFETs):
- ✅ **Pull-up resistors on BOTH sides** (10kΩ to 5V and 10kΩ to 3.3V)
- ✅ Works with simple GPIO configuration (INPUT/OUTPUT, no special pull-up handling)
- ✅ 17 discrete components (MOSFETs + resistors)

### Your Design (TXB0108 IC):
- ❌ **Missing pull-up resistors on 5V side** (this is the problem!)
- ✅ TXB0108 requires pull-ups on BOTH sides (per datasheet)
- ✅ Single IC instead of 17 discrete components

## Conclusion

**The original design confirms our theory!**

The original amigahid-pico design uses **pull-up resistors on BOTH the 5V and 3.3V sides** for each signal. This is exactly what the TXB0108 datasheet requires.

**Your TXB0108 design needs the same pull-up resistors on the 5V side** to work correctly. The original design uses 10kΩ resistors, which matches the TXB0108 datasheet recommendation (10kΩ to 50kΩ).

## Recommendation

Add **10kΩ to 22kΩ pull-up resistors** from each TXB0108 B-side pin (5V side) to 5V power supply, just like the original BSS138 design has. This will make your TXB0108 design work the same way as the original BSS138 design.

