# Button 2 and 3 GPIO Analysis

## GPIO Assignments (Revision 5)

### Port 1 Buttons 2 and 3:
- **Button 2**: GPIO 2 (`QM1_AMIGA_B2`)
- **Button 3**: GPIO 3 (`QM1_AMIGA_B3`)

### Port 2 Buttons 2 and 3:
- **Button 2**: GPIO 27 (`QM2_AMIGA_B2`)
- **Button 3**: GPIO 28 (`QM2_AMIGA_B3`)

## Critical Issue Found: Incorrect Comments in `joystick_port2.c`

**File**: `src/platform/amiga/joystick_port2.c` (lines 54, 63)

**Problem**: The comments incorrectly state that GPIO 27 is used for UP direction, but:
- `QM2_AMIGA_V` (UP direction) is actually **GPIO 19** (from `JOY0_ATARI_UP`)
- `QM2_AMIGA_B2` (Button 2) is **GPIO 27**

**Actual GPIO Assignments** (from `config.h`):
- `JOY0_ATARI_UP = 19` → `QM2_AMIGA_V = 19` (UP direction)
- `QM2_AMIGA_B2 = 27` (Button 2)
- `QM2_AMIGA_B3 = 28` (Button 3)

**Impact**: The comments are misleading but the code is correct. The actual GPIO assignments match the config.

## GPIO Conflict Check

### GPIO 2 (Port 1 Button 2):
- ✅ Used only for: `QM1_AMIGA_B2` (Port 1 Button 2 / Mouse Right Button)
- ✅ Initialized in: `quad_mouse.c`, `joystick_port1.c`, `gpio_util.c`
- ✅ No conflicts found

### GPIO 3 (Port 1 Button 3):
- ✅ Used only for: `QM1_AMIGA_B3` (Port 1 Button 3 / Mouse Middle Button)
- ✅ Initialized in: `quad_mouse.c`, `joystick_port1.c`, `gpio_util.c`
- ✅ No conflicts found

### GPIO 27 (Port 2 Button 2):
- ⚠️ **POTENTIAL ISSUE**: Comment in `joystick_port2.c` line 54 says "GPIO 27 = UP"
- ✅ Actually used for: `QM2_AMIGA_B2` (Port 2 Button 2)
- ✅ NOT used for: UP direction (that's GPIO 19)
- ✅ Initialized in: `joystick_port2.c`, `gpio_util.c`
- ✅ No actual conflicts (comment is just wrong)

### GPIO 28 (Port 2 Button 3):
- ✅ Used only for: `QM2_AMIGA_B3` (Port 2 Button 3)
- ✅ Initialized in: `joystick_port2.c`, `gpio_util.c`
- ✅ No conflicts found

## Initialization Order

All buttons 2 and 3 are initialized in the same order:
1. `amiga_gpio_reset_all_to_input()` (startup, in `main.c`)
2. `amiga_quad_mouse_init()` (Port 1 buttons, in `quad_mouse.c`)
3. `amiga_joystick_port1_init()` (Port 1 buttons, in `joystick_port1.c`)
4. `amiga_joystick_port2_init()` (Port 2 buttons, in `joystick_port2.c`)

All use `amiga_gpio_init_active_low()` which:
- Sets GPIO to INPUT mode with pull-up enabled (inactive state)
- Uses `GPIO_FUNC_SIO` (Software I/O)
- No special handling needed for ADC pins (GPIOs 26, 27, 28)

## Watchdog Coverage

✅ **FIXED**: Buttons 2 and 3 are now included in watchdog sample:
- `QM1_AMIGA_B2` (GPIO 2)
- `QM1_AMIGA_B3` (GPIO 3)
- `QM2_AMIGA_B2` (GPIO 27)
- `QM2_AMIGA_B3` (GPIO 28)

## Code Paths for Buttons 2 and 3

### Port 1 Button 2 (GPIO 2):
1. **Mouse Right Button**: `amiga_quad_mouse_button(AQM_RIGHT, ...)` → `amiga_gpio_set_active_low(QM1_AMIGA_B2, ...)`
2. **Joystick Button 2**: `amiga_joystick_port1_set_button(AJ1_BUTTON2, ...)` → `amiga_gpio_set_active_low(QM1_AMIGA_B2, ...)`

### Port 1 Button 3 (GPIO 3):
1. **Mouse Middle Button**: `amiga_quad_mouse_button(AQM_MIDDLE, ...)` → `amiga_gpio_set_active_low(QM1_AMIGA_B3, ...)`
2. **Joystick Button 3**: `amiga_joystick_port1_set_button(AJ1_BUTTON3, ...)` → `amiga_gpio_set_active_low(QM1_AMIGA_B3, ...)`

### Port 2 Button 2 (GPIO 27):
1. **Gamepad Button B**: `amiga_joystick_port2_set_button(AJ2_BUTTON2, ...)` → `amiga_gpio_set_active_low(QM2_AMIGA_B2, ...)`

### Port 2 Button 3 (GPIO 28):
1. **Gamepad Button X/Y**: `amiga_joystick_port2_set_button(AJ2_BUTTON3, ...)` → `amiga_gpio_set_active_low(QM2_AMIGA_B3, ...)`

## Recommendations

1. **Fix misleading comment** in `joystick_port2.c` line 54 - it says GPIO 27 = UP but should say GPIO 19 = UP
2. **Verify hardware connections** - ensure GPIOs 2, 3, 27, 28 are correctly wired to level shifters
3. **Test with LED** - attach LED to 5V side of level shifter to verify signals are reaching the hardware

