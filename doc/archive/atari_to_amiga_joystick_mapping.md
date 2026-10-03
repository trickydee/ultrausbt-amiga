# Atari to Amiga Joystick Port Mapping

This document maps the Atari ST joystick port GPIO pins to equivalent Amiga joystick port GPIO pins, enabling the use of Atari hardware to interface with Amiga systems.

## Atari ST Joystick Port GPIO Definitions

Based on `ultrausbt-atari-st-rpikbd/include/config.h`:

### Joystick 1 (Atari)
| Function | GPIO Pin | Signal Name |
|----------|----------|-------------|
| UP | 10 | `JOY1_UP` |
| DOWN | 11 | `JOY1_DOWN` |
| LEFT | 12 | `JOY1_LEFT` |
| RIGHT | 13 | `JOY1_RIGHT` |
| FIRE | 14 | `JOY1_FIRE` |

### Joystick 0 (Atari)
| Function | GPIO Pin | Signal Name |
|----------|----------|-------------|
| UP | 19 | `JOY0_UP` |
| DOWN | 20 | `JOY0_DOWN` |
| LEFT | 21 | `JOY0_LEFT` |
| RIGHT | 22 | `JOY0_RIGHT` |
| FIRE | 26 | `JOY0_FIRE` |

**Note:** Atari joystick ports are configured as **inputs with pull-up resistors** (active low - LOW = pressed, HIGH = not pressed).

---

## Amiga Joystick Port GPIO Definitions (Revision 5)

Based on `amigahid-pico/src/config.h`:

### Joystick Port 1 (Amiga)
| Function | DB-9 Pin | GPIO Pin | Signal Name | Direction |
|----------|----------|----------|-------------|-----------|
| UP | 1 | 10 | `QM1_AMIGA_V` | UP |
| DOWN | 2 | 9 | `QM1_AMIGA_H` | DOWN |
| LEFT | 3 | 8 | `QM1_AMIGA_VQ` | LEFT |
| RIGHT | 4 | 7 | `QM1_AMIGA_HQ` | RIGHT |
| FIRE | 6 | 11 | `QM1_AMIGA_B1` | - |
| BUTTON2 | 9 | 12 | `QM1_AMIGA_B2` | - |
| BUTTON3 | 5 | 13 | `QM1_AMIGA_B3` | - |

### Joystick Port 2 (Amiga)
| Function | DB-9 Pin | GPIO Pin | Signal Name | Direction |
|----------|----------|----------|-------------|-----------|
| UP | 1 | 27 | `QM2_AMIGA_V` | UP |
| DOWN | 2 | 26 | `QM2_AMIGA_H` | DOWN |
| LEFT | 3 | 22 | `QM2_AMIGA_VQ` | LEFT |
| RIGHT | 4 | 21 | `QM2_AMIGA_HQ` | RIGHT |
| FIRE | 6 | 20 | `QM2_AMIGA_B1` | - |
| BUTTON2 | 9 | 19 | `QM2_AMIGA_B2` | - |
| BUTTON3 | 5 | 18 | `QM2_AMIGA_B3` | - |

**Note:** Amiga joystick ports are configured as **outputs** (active low - LOW = active/pressed, HIGH = inactive/released).

---

## Mapping Table: Atari GPIO to Amiga GPIO

### Atari Joystick 1 → Amiga Port 1

| Atari Function | Atari GPIO | Amiga Function | Amiga GPIO | Amiga DB-9 Pin | Notes |
|----------------|------------|----------------|------------|----------------|-------|
| UP | 10 | UP | 10 | 1 | **Direct match** - Same GPIO pin! |
| DOWN | 11 | DOWN | 9 | 2 | Different GPIO |
| LEFT | 12 | LEFT | 8 | 3 | Different GPIO |
| RIGHT | 13 | RIGHT | 7 | 4 | Different GPIO |
| FIRE | 14 | FIRE | 11 | 6 | Different GPIO |

**Summary:** Only UP direction uses the same GPIO (10). All other signals require GPIO remapping.

### Atari Joystick 0 → Amiga Port 2

| Atari Function | Atari GPIO | Amiga Function | Amiga GPIO | Amiga DB-9 Pin | Notes |
|----------------|------------|----------------|------------|----------------|-------|
| UP | 19 | UP | 27 | 1 | Different GPIO |
| DOWN | 20 | DOWN | 26 | 2 | Different GPIO |
| LEFT | 21 | LEFT | 22 | 3 | Different GPIO |
| RIGHT | 22 | RIGHT | 21 | 4 | Different GPIO |
| FIRE | 26 | FIRE | 20 | 6 | Different GPIO |

**Summary:** No direct GPIO matches. All signals require GPIO remapping.

---

## Complete Cross-Reference Table

### By Direction/Function

| Function | Atari JOY1 GPIO | Atari JOY0 GPIO | Amiga Port 1 GPIO | Amiga Port 2 GPIO | Amiga DB-9 Pin |
|----------|-----------------|----------------|-------------------|----------------|----------------|
| **UP** | 10 | 19 | 10 | 27 | 1 |
| **DOWN** | 11 | 20 | 9 | 26 | 2 |
| **LEFT** | 12 | 21 | 8 | 22 | 3 |
| **RIGHT** | 13 | 22 | 7 | 21 | 4 |
| **FIRE** | 14 | 26 | 11 | 20 | 6 |

### By GPIO Pin (All Ports)

| GPIO Pin | Atari JOY1 | Atari JOY0 | Amiga Port 1 | Amiga Port 2 | Notes |
|----------|------------|------------|--------------|--------------|-------|
| 7 | - | - | RIGHT | - | Amiga Port 1 only |
| 8 | - | - | LEFT | - | Amiga Port 1 only |
| 9 | - | - | DOWN | - | Amiga Port 1 only |
| 10 | **UP** | - | **UP** | - | **Shared: Atari JOY1 UP = Amiga Port 1 UP** |
| 11 | DOWN | - | FIRE | - | Atari JOY1 DOWN vs Amiga Port 1 FIRE |
| 12 | LEFT | - | BUTTON2 | - | Atari JOY1 LEFT vs Amiga Port 1 BUTTON2 |
| 13 | RIGHT | - | BUTTON3 | - | Atari JOY1 RIGHT vs Amiga Port 1 BUTTON3 |
| 14 | FIRE | - | - | - | Atari JOY1 FIRE only |
| 18 | - | - | - | BUTTON3 | Amiga Port 2 only |
| 19 | - | **UP** | - | BUTTON2 | Atari JOY0 UP vs Amiga Port 2 BUTTON2 |
| 20 | - | DOWN | - | FIRE | Atari JOY0 DOWN vs Amiga Port 2 FIRE |
| 21 | - | LEFT | - | RIGHT | Atari JOY0 LEFT vs Amiga Port 2 RIGHT |
| 22 | - | RIGHT | - | LEFT | Atari JOY0 RIGHT vs Amiga Port 2 LEFT |
| 26 | - | FIRE | - | DOWN | Atari JOY0 FIRE vs Amiga Port 2 DOWN |
| 27 | - | - | - | UP | Amiga Port 2 only |

---

## Signal Logic Comparison

### Atari ST Joystick Ports
- **Configuration:** Input with pull-up resistors (`gpio_pull_up`)
- **Active State:** LOW (0) = pressed/active
- **Inactive State:** HIGH (1) = not pressed/inactive
- **Reading:** `gpio_get()` returns 0 when pressed, 1 when not pressed

### Amiga Joystick Ports
- **Configuration:** Output (active low)
- **Active State:** LOW (0) = direction pressed/button pressed
- **Inactive State:** HIGH (1) or input mode = direction released/button released
- **Writing:** `gpio_put()` or `amiga_gpio_set_active_low()` sets LOW for active, HIGH for inactive

**Key Difference:** Atari reads joystick state (input), Amiga writes joystick state (output). The logic is compatible (both active low), but the direction is opposite (read vs write).

---

## Implementation Considerations

### GPIO Conflicts

Several GPIO pins are used by different functions on different ports:

1. **GPIO 10**: 
   - Atari JOY1 UP = Amiga Port 1 UP
   - **No conflict** - Same function, same GPIO

2. **GPIO 11**:
   - Atari JOY1 DOWN (input)
   - Amiga Port 1 FIRE (output)
   - **Conflict** - Different functions, same GPIO

3. **GPIO 12**:
   - Atari JOY1 LEFT (input)
   - Amiga Port 1 BUTTON2 (output)
   - **Conflict** - Different functions, same GPIO

4. **GPIO 13**:
   - Atari JOY1 RIGHT (input)
   - Amiga Port 1 BUTTON3 (output)
   - **Conflict** - Different functions, same GPIO

5. **GPIO 19**:
   - Atari JOY0 UP (input)
   - Amiga Port 2 BUTTON2 (output)
   - **Conflict** - Different functions, same GPIO

6. **GPIO 20**:
   - Atari JOY0 DOWN (input)
   - Amiga Port 2 FIRE (output)
   - **Conflict** - Different functions, same GPIO

7. **GPIO 21**:
   - Atari JOY0 LEFT (input)
   - Amiga Port 2 RIGHT (output)
   - **Conflict** - Different functions, same GPIO

8. **GPIO 22**:
   - Atari JOY0 RIGHT (input)
   - Amiga Port 2 LEFT (output)
   - **Conflict** - Different functions, same GPIO

9. **GPIO 26**:
   - Atari JOY0 FIRE (input)
   - Amiga Port 2 DOWN (output)
   - **Conflict** - Different functions, same GPIO

### Solution Approach

To use Atari hardware with Amiga software, you would need to:

1. **Read Atari GPIO inputs** (joystick state from Atari hardware)
2. **Map to Amiga functions** (translate Atari joystick signals to Amiga joystick signals)
3. **Write to Amiga GPIO outputs** (drive Amiga joystick port signals)

**Example Mapping Logic:**
- Read `gpio_get(JOY1_UP)` (Atari GPIO 10) → Write to `amiga_gpio_set_active_low(QM1_AMIGA_V, !gpio_get(JOY1_UP))` (Amiga GPIO 10)
- Read `gpio_get(JOY1_DOWN)` (Atari GPIO 11) → Write to `amiga_gpio_set_active_low(QM1_AMIGA_H, !gpio_get(JOY1_DOWN))` (Amiga GPIO 9)
- Read `gpio_get(JOY1_LEFT)` (Atari GPIO 12) → Write to `amiga_gpio_set_active_low(QM1_AMIGA_VQ, !gpio_get(JOY1_LEFT))` (Amiga GPIO 8)
- Read `gpio_get(JOY1_RIGHT)` (Atari GPIO 13) → Write to `amiga_gpio_set_active_low(QM1_AMIGA_HQ, !gpio_get(JOY1_RIGHT))` (Amiga GPIO 7)
- Read `gpio_get(JOY1_FIRE)` (Atari GPIO 14) → Write to `amiga_gpio_set_active_low(QM1_AMIGA_B1, !gpio_get(JOY1_FIRE))` (Amiga GPIO 11)

**Note:** The `!` (NOT) operator is needed because:
- Atari: `gpio_get()` returns 0 when pressed, 1 when not pressed
- Amiga: `amiga_gpio_set_active_low()` expects true when active (LOW), false when inactive (HIGH)
- So: `!gpio_get()` converts 0→true (active) and 1→false (inactive)

---

## Recommended Mapping Strategy

### Option 1: Direct GPIO Mapping (Where Possible)
- **Atari JOY1 UP → Amiga Port 1 UP**: Direct (GPIO 10)
- All other signals require GPIO remapping

### Option 2: Logical Port Mapping
- **Atari JOY1 → Amiga Port 1**: Map all signals with GPIO translation
- **Atari JOY0 → Amiga Port 2**: Map all signals with GPIO translation

### Option 3: Hardware-Level Mapping
- Use a hardware adapter/PCB that routes Atari joystick signals to the correct Amiga GPIO pins
- This would allow using the Atari hardware board directly with minimal software changes

---

## Code References

### Atari Project
- GPIO definitions: `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultrausbt-atari-st-rpikbd/include/config.h`
- Joystick handling: `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultrausbt-atari-st-rpikbd/src/HidInput.cpp` (lines 517-526, 2034-2047)

### Amiga Project
- GPIO definitions: `src/config.h` (Revision 5)
- Joystick Port 1: `src/platform/amiga/joystick_port1.c`
- Joystick Port 2: `src/platform/amiga/joystick_port2.c`

---

## Notes

1. **GPIO Direction**: Atari joystick ports are **inputs** (read joystick state), while Amiga joystick ports are **outputs** (drive joystick signals). This requires software translation.

2. **Active Low Logic**: Both systems use active low logic, which simplifies translation but requires inverting the signal (Atari: 0=pressed, Amiga: 0=active).

3. **GPIO Conflicts**: Most GPIO pins conflict between Atari and Amiga functions. Only GPIO 10 (UP on both Atari JOY1 and Amiga Port 1) is a direct match.

4. **Hardware Considerations**: The Atari board's physical joystick connectors may have different pinouts than the Amiga DB-9 connectors. This mapping only covers GPIO assignments, not physical connector pinouts.

5. **Future Work**: Keyboard pinout mapping will be investigated separately as requested.

