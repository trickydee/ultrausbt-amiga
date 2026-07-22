# Electrical Operation of the Amiga Joystick Adapter

This document explains how the amigahid-pico adapter interfaces with the Amiga joystick port at an electrical level.

## 1. Amiga Joystick Port Electrical Characteristics

The Amiga joystick port uses **active-low logic** with pull-up resistors:

- **Voltage levels**: 5V logic (Amiga) vs 3.3V logic (RP2040)
- **Signal logic**: **Active low** (LOW = pressed/active, HIGH = released/inactive)
- **Pull-up resistors**: The Amiga has internal pull-up resistors (typically ~10kΩ) that pull signals HIGH when nothing is connected
- **Input impedance**: The Amiga reads these as digital inputs

## 2. How the RP2040 Interfaces Electrically

The adapter uses a clever technique to drive 5V signals from 3.3V GPIO:

```
When INACTIVE (button/direction released):
  - RP2040 GPIO configured as INPUT (high impedance)
  - Amiga's internal pull-up resistor pulls the line HIGH (5V)
  - Amiga sees: HIGH = inactive/released ✓

When ACTIVE (button/direction pressed):
  - RP2040 GPIO configured as OUTPUT
  - RP2040 drives the line LOW (0V / GND)
  - Amiga sees: LOW = active/pressed ✓
```

## 3. Electrical Implementation Details

From `gpio_util.c`:

```c
void amiga_gpio_set_active_low(uint32_t gpio, bool active)
{
    if (active) {
        // ACTIVE: Drive LOW (0V) by setting as OUTPUT and writing 0
        gpio_put(gpio, 0);           // Drive to 0V
        gpio_set_dir(gpio, GPIO_OUT); // Enable output driver
    } else {
        // INACTIVE: Let Amiga pull-up do the work
        gpio_set_dir(gpio, GPIO_IN);  // High impedance (tristate)
        // Amiga's pull-up resistor pulls line to 5V
    }
}
```

## 4. Electrical Circuit Diagram (Simplified)

```
Amiga Joystick Port          RP2040 GPIO Pin
     (5V logic)              (3.3V logic)
         |                         |
    [10kΩ]                    [GPIO]
    Pull-up                      |
         |                       |
    ┌────┴────┐              ┌───┴───┐
    │  Amiga  │              │ RP2040│
    │  Input  │◄─────────────┤ GPIO  │
    │  Pin    │              │       │
    └─────────┘              └───────┘
         |                         |
        GND                       GND
```

## 5. Voltage Level Compatibility

- **RP2040 GPIO**: 3.3V tolerant, can sink current to GND
- **Amiga input**: 5V TTL logic, reads LOW as < 0.8V, HIGH as > 2.0V
- **Compatibility**: When RP2040 drives LOW (0V), Amiga reads it as LOW. When RP2040 is high-Z, Amiga's pull-up pulls to 5V, which the RP2040 can tolerate (it's 5V-tolerant as input).

## 6. Example: Pressing UP Direction

**Electrical sequence:**

1. **Initial state (UP not pressed):**
   - RP2040: GPIO 19 (UP) = INPUT (high-Z)
   - Amiga: Line pulled HIGH (5V) by pull-up
   - Amiga reads: HIGH = UP not pressed

2. **User presses UP:**
   - Software: `amiga_joystick_port2_set_direction(AJ2_UP, true)`
   - RP2040: GPIO 19 = OUTPUT, drives LOW (0V)
   - Amiga: Line pulled LOW (0V)
   - Amiga reads: LOW = UP pressed

3. **User releases UP:**
   - Software: `amiga_joystick_port2_set_direction(AJ2_UP, false)`
   - RP2040: GPIO 19 = INPUT (high-Z)
   - Amiga: Line pulled HIGH (5V) by pull-up
   - Amiga reads: HIGH = UP not pressed

## 7. Current Flow

**When active (driving LOW):**
- Current flows: Amiga 5V → Pull-up resistor (10kΩ) → RP2040 GPIO (sinking to GND)
- Current: ~0.5mA per pin (5V / 10kΩ)
- RP2040 GPIO can sink up to ~12mA, so this is safe

## 8. Why This Works

1. **No level shifter needed**: The RP2040 doesn't need to drive HIGH; it only drives LOW or floats
2. **Pull-up does the work**: The Amiga's pull-up provides the HIGH state
3. **Low power**: Only draws current when active (driving LOW)
4. **Fast switching**: GPIO can switch between input/output quickly

## 9. Safety Considerations

- **No 5V on RP2040**: The RP2040 never drives 5V; it only sinks to GND or floats
- **Input protection**: RP2040 GPIOs are 5V-tolerant as inputs, so the 5V pull-up is safe
- **Current limiting**: The 10kΩ pull-up limits current to safe levels

---

## Risks to the Amiga

### Low Risk Scenarios

1. **Normal Operation**
   - **Risk Level**: Very Low
   - **Description**: During normal operation, the adapter only sinks current to GND when active, which is exactly what a physical joystick would do
   - **Mitigation**: The design mimics standard joystick behavior

2. **GPIO Configuration Errors**
   - **Risk Level**: Low
   - **Description**: If a GPIO is accidentally configured as OUTPUT and driven HIGH (3.3V), the Amiga would see this as a valid HIGH signal (3.3V > 2.0V threshold)
   - **Mitigation**: The code never drives HIGH; it only drives LOW or floats

3. **Software Bugs**
   - **Risk Level**: Low
   - **Description**: A bug could cause multiple directions to be active simultaneously (e.g., UP and DOWN at the same time)
   - **Impact**: This would be equivalent to a faulty joystick - the Amiga would read conflicting inputs, but no electrical damage would occur
   - **Mitigation**: The code tracks state and prevents conflicts

### Potential Risks (Theoretical)

1. **Short Circuit Protection**
   - **Risk Level**: Very Low (theoretical)
   - **Description**: If the RP2040 GPIO output driver fails short to GND permanently, it would continuously sink current
   - **Impact**: Would draw ~0.5mA continuously (negligible)
   - **Mitigation**: RP2040 GPIOs have built-in protection diodes and current limiting

2. **Overcurrent Protection**
   - **Risk Level**: Very Low
   - **Description**: The 10kΩ pull-up resistor limits current to ~0.5mA per pin, well below the RP2040's 12mA sink capability
   - **Mitigation**: Built-in current limiting in RP2040 GPIO drivers

3. **Voltage Spikes**
   - **Risk Level**: Very Low
   - **Description**: The Amiga's 5V supply could have voltage spikes during power-on or power-off
   - **Impact**: RP2040 GPIOs are 5V-tolerant as inputs (can handle up to 3.3V + 0.3V = 3.6V, but datasheet shows they can tolerate 5V as inputs)
   - **Mitigation**: When configured as INPUT, the GPIO is high-impedance and protected

4. **Ground Potential Differences**
   - **Risk Level**: Very Low
   - **Description**: If the Amiga and RP2040 have different ground potentials, current could flow through signal lines
   - **Impact**: Minimal - both devices share the same ground through the connector
   - **Mitigation**: Proper grounding through the connector

### Real-World Safety Assessment

**Overall Risk to Amiga: VERY LOW**

**Reasons:**
1. **Current limiting**: The 10kΩ pull-up resistors limit current to ~0.5mA per pin, which is negligible
2. **Standard behavior**: The adapter behaves exactly like a physical joystick (sinks to GND when active)
3. **No high voltage**: The adapter never drives voltages higher than 3.3V
4. **Input protection**: RP2040 GPIOs have built-in protection
5. **Isolation**: When inactive, the GPIO is high-impedance, providing electrical isolation

**Comparison to Physical Joystick:**
- A physical joystick shorts the signal line to GND when pressed (same as adapter)
- A physical joystick leaves the line floating when released (same as adapter when GPIO is INPUT)
- The adapter behaves identically to a physical joystick from the Amiga's perspective

### Recommendations

1. **Power sequencing**: Ensure the Amiga is powered on before the adapter (or vice versa) - no specific order required, but avoid hot-plugging during operation
2. **Ground connection**: Ensure proper ground connection between Amiga and adapter
3. **Firmware updates**: Keep firmware updated to avoid software bugs that could cause unexpected behavior
4. **Testing**: Test with a non-critical Amiga first if concerned

### Conclusion

The electrical design is **very safe for the Amiga**. The adapter:
- Never drives high voltages
- Limits current through pull-up resistors
- Behaves identically to a physical joystick
- Uses standard digital I/O techniques

The risk of damage to the Amiga is **extremely low** and comparable to using a standard physical joystick.

