# Troubleshooting: Intermittent Mouse/Joystick Failure

## Symptom
Both mouse and joystick stopped working on the Amiga, but started working again after unplugging and replugging the Pico adapter.

## Potential Causes and Solutions

### 1. Power Supply Issues (MOST LIKELY)

**Problem**: The Pico is currently powered by USB, not from the Amiga's 5V controller port.

**Why this matters:**
- **Ground potential differences**: If the Pico (USB-powered) and Amiga (separate power supply) have different ground potentials, signals may not work correctly
- **Power sequencing**: If the Amiga powers on before the Pico, or vice versa, GPIO states might be incorrect
- **USB power instability**: USB power can be unstable, especially if:
  - USB port is low-power (some USB 2.0 ports)
  - USB cable is long or poor quality
  - USB hub is used
  - Computer goes to sleep/suspends

**Solutions:**
1. **Power from Amiga controller port** (RECOMMENDED):
   - Connect the Pico's VSYS pin to the Amiga's +5V from the controller port
   - Connect GND from Pico to Amiga GND
   - This ensures both devices share the same power and ground
   - **Note**: Ensure proper voltage regulation (Amiga 5V → Pico VSYS, which accepts 1.8V-5.5V)

2. **Use a powered USB hub**:
   - If you must use USB power, use a powered USB hub to ensure stable 5V

3. **Check USB cable quality**:
   - Use a high-quality USB cable with proper power delivery
   - Avoid long or thin cables

### 2. GPIO State Corruption

**Problem**: GPIO pins might get stuck in an incorrect state due to:
- Software bug causing incorrect GPIO configuration
- Power glitch corrupting GPIO state
- Hot-plugging causing initialization issues

**Symptoms:**
- GPIOs stuck as OUTPUT when they should be INPUT (or vice versa)
- GPIO direction cache getting out of sync with actual hardware state

**Solutions:**
1. **Add GPIO reset function**:
   ```c
   void amiga_gpio_reset_all(void) {
       // Reset all Amiga GPIOs to known state
       amiga_joystick_port1_reset();
       amiga_joystick_port2_reset();
       // Re-initialize mouse GPIOs
       amiga_quad_mouse_init();
   }
   ```

2. **Clear GPIO direction cache on reset**:
   - The `gpio_dir_cache` in `gpio_util.c` could get out of sync
   - Add a function to clear the cache and re-initialize

3. **Add watchdog/reset capability**:
   - Monitor GPIO state periodically
   - Reset if stuck in incorrect state

### 3. Hot-Plugging Issues

**Problem**: Connecting/disconnecting the Pico while the Amiga is powered can cause:
- GPIO pins to be driven by the Amiga's pull-ups before Pico initializes
- Initialization race conditions
- GPIO state conflicts

**Solutions:**
1. **Power sequencing**:
   - Always power on the Amiga first, then connect the Pico
   - Or: Connect Pico first, then power on Amiga
   - Avoid hot-plugging during operation

2. **Add initialization delay**:
   - Add a small delay after power-on before initializing GPIOs
   - This allows power to stabilize

### 4. Voltage Level Issues

**Problem**: While the design should work (3.3V Pico driving 5V Amiga signals), edge cases might occur:
- If Pico GPIO is accidentally driven HIGH (3.3V), Amiga might read it as HIGH (works, but not ideal)
- If there's a voltage spike or noise, it could cause issues

**Solutions:**
1. **Add pull-up resistors on hardware** (if not already present):
   - External 10kΩ pull-ups on Amiga side ensure clean HIGH state
   - Amiga should have internal pull-ups, but external ones add redundancy

2. **Add input protection**:
   - Series resistors (100-220Ω) on signal lines can protect against voltage spikes
   - Schottky diodes for overvoltage protection (if needed)

### 5. Initialization Order Issues

**Problem**: The initialization order in `main.c` might cause conflicts:
- Mouse and joystick port 1 share the same GPIOs
- If both try to initialize the same pins, conflicts can occur

**Current initialization order:**
```c
amiga_quad_mouse_init();      // Initializes Port 1 GPIOs
amiga_joystick_port1_init();  // Re-initializes same GPIOs
amiga_joystick_port2_init();  // Initializes Port 2 GPIOs
```

**Solutions:**
1. **Ensure proper initialization order**:
   - Both mouse and joystick port 1 initialize the same pins
   - This should be fine, but ensure they both set pins to INPUT initially

2. **Add conflict detection**:
   - Check if GPIOs are already initialized before re-initializing
   - Log warnings if conflicts detected

### 6. USB Host Stack Issues

**Problem**: If the USB host stack (TinyUSB) has issues, it might affect the main loop:
- `tuh_task()` blocking or hanging
- USB enumeration issues causing delays
- This could make the adapter appear "dead" even though GPIOs are working

**Solutions:**
1. **Add timeout handling**:
   - Ensure `tuh_task()` doesn't block indefinitely
   - Add watchdog to detect if main loop stops

2. **Monitor USB state**:
   - Log USB events to help diagnose issues

## Recommended Immediate Actions

### 1. Power from Amiga (BEST SOLUTION)
**Hardware modification:**
- Connect Amiga +5V (from controller port) to Pico VSYS pin
- Connect Amiga GND to Pico GND
- Keep USB connected for data only (or use separate USB data connection)

**Benefits:**
- ✅ Shared power and ground eliminates potential differences
- ✅ More stable power supply
- ✅ Proper power sequencing
- ✅ No USB power issues

### 2. Add GPIO Reset/Clear Function

**After Power-Up (Recommended)**:
Add a function to explicitly clear/reset all GPIOs before initialization. This ensures a clean state even if the Amiga is already powered:

```c
void amiga_gpio_clear_all_before_init(void) {
    // Clear GPIO direction cache
    for (int i = 0; i < 32; i++) {
        amiga_gpio_clear_cache(i);
    }
    
    // Explicitly set all Amiga GPIOs to INPUT (inactive/high) before initialization
    // This ensures clean state even if Amiga is already powered
    amiga_gpio_init_active_low(QM1_AMIGA_H, false);
    amiga_gpio_init_active_low(QM1_AMIGA_V, false);
    amiga_gpio_init_active_low(QM1_AMIGA_HQ, false);
    amiga_gpio_init_active_low(QM1_AMIGA_VQ, false);
    amiga_gpio_init_active_low(QM1_AMIGA_B1, false);
    amiga_gpio_init_active_low(QM1_AMIGA_B2, false);
    amiga_gpio_init_active_low(QM1_AMIGA_B3, false);
    
    amiga_gpio_init_active_low(QM2_AMIGA_H, false);
    amiga_gpio_init_active_low(QM2_AMIGA_V, false);
    amiga_gpio_init_active_low(QM2_AMIGA_HQ, false);
    amiga_gpio_init_active_low(QM2_AMIGA_VQ, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B1, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B2, false);
    amiga_gpio_init_active_low(QM2_AMIGA_B3, false);
}
```

Call this in `main()` before initializing mouse/joystick ports, with a small delay:

```c
// Wait for power to stabilize (especially if Amiga is already powered)
sleep_ms(100);

// Clear all GPIOs to known state before initialization
amiga_gpio_clear_all_before_init();

// Now initialize normally
amiga_quad_mouse_init();
amiga_joystick_port1_init();
amiga_joystick_port2_init();
```

**During Use (Recovery)**:
Add a watchdog function to detect and recover from stuck GPIO states:

```c
void amiga_gpio_recover_if_stuck(void) {
    // Check if GPIOs are stuck in wrong state
    // If detected, reset them
    // This would be called periodically from main loop
}
```

### 3. Add Diagnostic Logging
Add serial output to help diagnose issues:
- Log GPIO state on initialization
- Log if GPIO state changes unexpectedly
- Log power-on sequence

### 4. Add Watchdog
Monitor that the main loop is running and GPIOs are responding:
- If no input for extended period, reset GPIOs
- Detect stuck GPIO states

## Testing Recommendations

1. **Power source test**:
   - Try powering from Amiga controller port instead of USB
   - See if issue persists

2. **Power sequencing test**:
   - Try different power-on sequences:
     - Amiga first, then Pico
     - Pico first, then Amiga
     - Both simultaneously
   - Note which sequence works best

3. **USB cable test**:
   - Try different USB cables
   - Try different USB ports
   - Try powered USB hub

4. **Monitor serial output**:
   - Connect serial console to see if Pico is still running
   - Check for error messages
   - Verify initialization messages

## Long-term Solutions

1. **Hardware improvements**:
   - Add proper power management (power from Amiga)
   - Add input protection (series resistors, protection diodes)
   - Add status LED to indicate Pico is running

2. **Software improvements**:
   - Add GPIO state monitoring
   - Add automatic recovery from stuck states
   - Add watchdog timer
   - Improve error handling and logging

3. **Documentation**:
   - Document recommended power setup
   - Document known issues and workarounds
   - Add troubleshooting guide

## Most Likely Causes (with Quality AC-USB Supply)

Since you're using a good quality AC-USB supply, USB power instability is less likely. The most likely causes are:

### 1. Ground Potential Differences (MOST LIKELY)

**Problem**: Even with good USB power, if the Pico (USB-powered) and Amiga (separate PSU) have different ground potentials, signals may not work correctly.

**Why this happens**:
- USB power supply and Amiga PSU may have slight ground potential differences
- This can cause signals to be misread or not work reliably
- Intermittent because it depends on power supply state, load, etc.

**Solution**: Power the Pico from the Amiga's controller port +5V instead of USB. This ensures:
- Shared power and ground (eliminates potential differences)
- More stable operation
- Proper power sequencing

### 2. GPIO Initialization Race Condition

**Problem**: If the Amiga is already powered when the Pico boots:
- Amiga's pull-up resistors pull lines HIGH immediately
- Pico initializes GPIOs, but there's a race condition
- Mouse and joystick port 1 both initialize the same GPIOs (potential conflict)

**Solution**: Add explicit GPIO reset/clear before initialization:
- Clear GPIO direction cache
- Add small delay to let power stabilize
- Ensure all GPIOs are explicitly set to INPUT before use

### 3. Hot-Plugging Issues

**Problem**: Connecting the Pico while the Amiga is already powered can cause:
- GPIOs to be in unknown state during initialization
- Initialization order issues

**Solution**: 
- Always power on in consistent order (Amiga first, then Pico, or vice versa)
- Add delay after power-on before GPIO initialization

## Additional Notes

- The 3.3V vs 5V voltage difference should not be an issue (as documented in `electrical_operation.md`)
- The design is electrically safe for the Amiga
- The issue is likely power/initialization related, not voltage level related

