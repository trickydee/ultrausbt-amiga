# GPIO Fixes and TODO

## Square Button (BUTTON_X) Causing Bluetooth Disconnect - FIXED

### Problem
Pressing the Square button (BUTTON_X) on DS5/Stadia controllers causes the Bluetooth connection to disconnect and the device to crash.

### Root Cause
**GPIO 18 Conflict:**
- `QM2_AMIGA_B3` (Joystick Port 2 Button 3) uses GPIO 18 - configured as OUTPUT
- `GPIO_BUTTON_LEFT` (Display left button) uses GPIO 18 - configured as INPUT with pull-up

When Square button is pressed:
1. Joystick code tries to set GPIO 18 as OUTPUT (active low)
2. Display code has already configured GPIO 18 as INPUT
3. GPIO conflict causes system instability/crash
4. Bluetooth stack crashes and disconnects

### Fix Applied (v1.0.4)
**Files Modified:**
- `src/platform/amiga/joystick_port2.c`: Disabled `AJ2_BUTTON3` handling
- `src/usb_hid.c`: Disabled Button 3 mapping for both USB and Bluetooth gamepads

**Changes:**
1. Commented out `amiga_gpio_set_active_low(QM2_AMIGA_B3, pressed)` in `joystick_port2.c`
2. Commented out `amiga_joystick_port2_set_button(AJ2_BUTTON3, ...)` calls in `usb_hid.c`
3. Added TODO comments documenting the conflict

### Current Button Mapping (Port 2)
- **BUTTON_A (X/Fire)**: ✅ Works - mapped to Port 2 Fire
- **BUTTON_B (Circle)**: ✅ Works - mapped to Port 2 Button 2
- **BUTTON_X (Square)**: ❌ Disabled - GPIO 18 conflict with display button
- **BUTTON_Y (Triangle)**: ❌ Disabled - same conflict

### Hardware Fix Required
To restore Button 3 functionality, **remap `QM2_AMIGA_B3` to a different GPIO** in hardware.

**Current:** `QM2_AMIGA_B3 = GPIO 18` (conflicts with `GPIO_BUTTON_LEFT`)

**Recommended:** Choose a free GPIO that doesn't conflict with:
- Display buttons (GPIO 16, 17, 18)
- Joystick Port 1 (GPIO 7-13)
- Joystick Port 2 (GPIO 18-22, 26-27)
- Keyboard (GPIO 4-6)
- I2C Display (GPIO 8-9)

### Related Issues
- **Circle Button Triggering UP**: Fixed in v1.0.3 - D-pad UP ignored when buttons are pressed (prevents HID standard hat value 0 = UP from interfering)

### Testing Notes
- Square button no longer causes crashes
- Circle button no longer triggers UP movement
- Fire button (A) works correctly
- Button 3 functionality disabled until hardware remap

### Version History
- **v1.0.3**: Added Circle button UP fix and debug logging
- **v1.0.4**: Fixed Square button GPIO conflict (disabled Button 3)

