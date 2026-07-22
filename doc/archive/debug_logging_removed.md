# Debug Logging Added for GPIO and Gamepad Troubleshooting

This document describes the debug logging that was added during troubleshooting of GPIO and gamepad issues, particularly for the fire button (GPIO 26) on Pico W and gamepad button mapping issues.

## Overview

Extensive debug logging was added to diagnose:
1. **GPIO 26 (Fire Button) Issues**: Why the fire button wasn't working on Pico W
2. **Gamepad Button Mapping**: Circle button triggering UP movement, Square button causing Bluetooth disconnects
3. **GPIO State Verification**: Ensuring GPIOs are set correctly and detecting hardware issues

## Debug Logging Locations

### 1. GPIO Utility (`src/platform/common/gpio_util.c`)

**Location**: `amiga_gpio_set_active_low()` function

**Purpose**: Track GPIO 26 (Port 2 Fire button) operations in detail

**Logs Added**:
- **Before Set**: Logs GPIO state before changing it (direction, level, target state)
  ```
  [GPIO-UTIL] GPIO 26 (Fire): before set - dir=INPUT level=1, setting to ACTIVE (OUTPUT/LOW)
  ```

- **After Set**: Logs GPIO state after setting (direction, output register value, actual pin state, expected state)
  ```
  [GPIO-UTIL] GPIO 26 (Fire): after set - dir=OUTPUT gpio_get()=1 pin_hw_state=1 (expected: OUTPUT/0)
  ```

- **Warnings**: 
  - If output register is not set correctly (GPIO is OUTPUT but `gpio_get()` returns 1 instead of 0)
  - If output register is 0 but pin is HIGH (external pull-up or hardware issue)

**What It Diagnosed**:
- Identified that `gpio_put(gpio, 0)` was not successfully setting GPIO 26 to LOW
- Revealed the need to explicitly disable pull-ups before setting to OUTPUT
- Helped identify potential hardware damage from 5V back-feeding

### 2. Joystick Port 2 (`src/platform/amiga/joystick_port2.c`)

**Location**: `amiga_joystick_port2_init()` and `amiga_joystick_port2_set_button()`

**Purpose**: Track fire button initialization and state changes

**Logs Added**:

#### Initialization (`amiga_joystick_port2_init()`):
```
[JOY2-INIT] GPIO 26 (Fire): dir=INPUT level=1 (should be INPUT=0, HIGH=1)
```

#### Fire Button State Changes (`amiga_joystick_port2_set_button()`):
- **Function Call**: Logs when function is called with current and previous state
  ```
  [JOY2-FIRE] Called: pressed=1 prev_button1=0 (GPIO 26)
  ```

- **State Change**: Logs when button state actually changes
  ```
  [JOY2-FIRE] State changed! Setting GPIO 26 to ACTIVE (LOW/OUTPUT)
  ```

- **After Set**: Logs GPIO state after setting and compares with expected state
  ```
  [JOY2-FIRE] After set: GPIO 26 dir=OUTPUT level=1 (expected: pressed=1 -> dir=OUTPUT level=0)
  ```

- **State Unchanged**: Logs when button state hasn't changed (optimization skip)
  ```
  [JOY2-FIRE] State unchanged, skipping GPIO update
  ```

#### Reset Function (`amiga_joystick_port2_reset()`):
```
[JOY2-RESET] prev_button1 before reset: 1
[JOY2-RESET] prev_button1 after reset: 0
```

**What It Diagnosed**:
- Confirmed function was being called correctly
- Verified `prev_button1` state tracking was working
- Identified that GPIO was being set but hardware wasn't responding
- Helped confirm hardware damage on first Pico W

### 3. Gamepad Debug (`src/usb_hid.c`)

**Location**: `process_bluepad32_gamepad()` function

**Purpose**: Track gamepad button states, D-pad, and analog stick values

**Logs Added**:
```
[GAMEPAD-DEBUG] buttons=0x0002 (A=0 B=1 X=0 Y=0) dpad=0x00 axis_x=4 axis_y=-8
```

**What It Logs**:
- `buttons`: Raw button bitmask (0x0001=A, 0x0002=B, 0x0004=X, 0x0008=Y)
- Individual button states: A, B, X, Y (0 or 1)
- `dpad`: D-pad state (0x01=UP, 0x02=DOWN, 0x04=RIGHT, 0x08=LEFT)
- `axis_x`, `axis_y`: Analog stick values (-128 to +127)

**When It Logs**:
- Only when button states or D-pad state changes (to reduce log spam)
- Triggered by comparing `last_buttons` and `last_dpad` with current values

**What It Diagnosed**:
- **Circle Button (B) Issue**: Revealed that when Circle was pressed, `dpad=0x01` (UP) was also reported, causing unintended UP movement
  - Root cause: Bluepad32's `uni_hid_parser_hat_to_dpad` maps hat value `0` (neutral) to `DPAD_UP` per HID standard
  - Fix: Filter out `DPAD_UP` if `BUTTON_B` is also pressed

- **Square Button (X) Issue**: Showed button was being pressed correctly, but caused Bluetooth disconnect
  - Root cause: GPIO conflict (GPIO 18 used by both Button 3 and Display Left Button)
  - Fix: Disabled Button 3 functionality until GPIO remapping

## Debug Logging Impact

### Performance Impact
- **GPIO Logging**: Only logs for GPIO 26 (fire button), minimal impact
- **Gamepad Logging**: Only logs on state changes, minimal impact
- **All logging uses `ahprintf()`**: Non-blocking, doesn't affect real-time performance

### Log Volume
- **Normal Operation**: Very few logs (only on button presses/releases)
- **Troubleshooting**: High volume when investigating issues (every button state change)
- **Production**: Should be removed to reduce log noise

## Removal

All debug logging was removed in v1.0.9 after confirming:
1. GPIO 26 issues were hardware-related (damaged Pico W from 5V back-feeding)
2. Gamepad button mapping issues were resolved (Circle button filter, GPIO remapping)
3. GPIO initialization safety was verified (all GPIOs start as INPUT)

## Lessons Learned

1. **GPIO 26 (ADC0) is sensitive**: Requires explicit `gpio_set_function(GPIO_FUNC_SIO)` to prevent ADC interference
2. **Pull-ups must be disabled for OUTPUT**: `gpio_set_pulls(gpio, false, false)` before setting to OUTPUT
3. **Hardware protection is critical**: 5V back-feeding can damage GPIOs if Pico is not powered
4. **Bluepad32 D-pad quirk**: Hat value `0` maps to `DPAD_UP`, requiring filtering when buttons are pressed
5. **GPIO conflicts cause crashes**: Multiple drivers on same GPIO can cause system instability

## Related Documentation

- `device_troubleshooting.md`: Details fixes for DS5 pairing, Stadia controller issues, and joystick port 1 fixes
- `gpio-fixes-todo.md`: Documents GPIO conflict fixes and remapping
- `amiga-code-atari-board-gpio-mappings.md`: Complete GPIO mapping documentation

