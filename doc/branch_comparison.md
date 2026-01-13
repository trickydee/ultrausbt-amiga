# Branch Comparison: bluetooth vs revision5-wip

## Summary

The `revision5-wip` branch was intended to add Revision 5 GPIO configuration support, but appears to have been reverted. However, based on the Bluetooth pairing issue reported, here's what likely changed and what may be causing the problem.

## Expected Differences (Revision 5 Changes)

### 1. CMakeLists.txt
- **bluetooth branch**: `HIDPICO_REVISION=4`
- **revision5-wip branch**: `HIDPICO_REVISION=5` (expected)

### 2. src/config.h
- **bluetooth branch**: Only Revision 2 and 4 GPIO definitions
- **revision5-wip branch**: Should have Revision 5 GPIO definitions added:
  - Same as Revision 4 for keyboard, mouse, and Port 1
  - New GPIO definitions for Joystick Port 2:
    - GPIO 18, 19, 20 (Buttons)
    - GPIO 21, 22 (Quadrature)
    - GPIO 26, 27 (Direction - FIXED from Revision 4)

### 3. src/CMakeLists.txt
- **Both branches**: Should be identical for Bluetooth support
- **Note**: Missing `pico_btstack_ble` library in both (may need to be added)

## Bluetooth Pairing Issue Analysis

### Symptom
- Bluetooth keyboard begins to pair
- Then becomes unpaired/disconnects

### Possible Causes

1. **GPIO Pin Conflicts (UNLIKELY)**
   - CYW43 uses dedicated SPI interface, not regular GPIOs
   - Revision 5 GPIO changes (18-27) shouldn't affect Bluetooth
   - **Conclusion**: Probably not the cause

2. **Missing Library (LIKELY)**
   - `pico_btstack_ble` library missing from linker
   - This provides GATT client functions needed for HID service discovery
   - Without it, pairing may start but fail during service discovery
   - **Fix**: Add `pico_btstack_ble` to `target_link_libraries` in `src/CMakeLists.txt`

3. **Initialization Timing**
   - Bluepad32 initialization might be happening too early/late
   - CYW43 chip reset sequence might need adjustment
   - **Check**: `bluepad32_init()` timing in `main.c`

4. **Configuration Issues**
   - `btstack_config.h` or `sdkconfig.h` might have incorrect settings
   - HID service discovery timeout too short
   - **Check**: `MAX_NR_HIDS_CLIENTS` and related settings

## Recommended Fixes

### Priority 1: Add Missing Library
```cmake
target_link_libraries(amigahid-pico PUBLIC
    pico_cyw43_arch_none
    pico_btstack_ble      # ADD THIS - Required for GATT client
    pico_btstack_classic
    pico_btstack_cyw43
    pico_async_context_poll
    bluepad32
)
```

### Priority 2: Check Initialization Order
Ensure Bluepad32 initializes after:
- CYW43 chip is ready
- Async context is set up
- Sufficient delay for chip stabilization

### Priority 3: Verify Configuration
Check `btstack_config.h`:
- `MAX_NR_HIDS_CLIENTS` should be >= 4
- `MAX_NR_GATT_CLIENTS` should be >= 4
- Timeout values are reasonable

## Current State

The `revision5-wip` branch appears to have been reverted to match `bluetooth` branch. To properly implement Revision 5:

1. Set `HIDPICO_REVISION=5` in `CMakeLists.txt`
2. Add Revision 5 GPIO definitions to `src/config.h`
3. Add `pico_btstack_ble` to fix Bluetooth pairing issue
4. Test Bluetooth pairing with the fix

