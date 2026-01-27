# DS5 DualSense Pairing Fix for Pico 2 W

## Problem Summary

The DualSense 5 (DS5) controller was failing to pair on Pico 2 W (RP2350) but worked correctly on Pico W (RP2040). The failure occurred during Bluetooth authentication, specifically during the SSP (Secure Simple Pairing) process.

### Symptoms
- DS5 discovered and connection initiated
- `HCI_EVENT_LINK_KEY_REQUEST` received
- `L2CAP Connection failed: 0x66` error
- `HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT: status=6` (failure)
- Never received `SSP User Confirmation Request`
- Authentication always failed, causing disconnection

### Working vs Failing Behavior

**Pico W (Working):**
- Received `SSP User Confirmation Request with numeric value '798941'`
- Auto-accepted confirmation
- `HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT: status=0` (success)
- L2CAP channels opened successfully
- Device connected and ready

**Pico 2 W (Failing):**
- Never received SSP confirmation request
- Authentication always failed with status=6
- Connection repeatedly attempted and failed

## Root Cause

The issue was **missing flash-safe execution coordination** between Core 0 (Bluetooth) and Core 1 (mouse processing).

### Why Flash Coordination Matters

1. **DS5 uses SSP (Secure Simple Pairing)** which requires writing link keys to flash memory during pairing
2. **Bluetooth TLV storage** uses flash to persist pairing keys across power cycles
3. **Core 1 (mouse processing)** runs independently and can access flash simultaneously
4. **Without coordination**, Core 1 can freeze or interfere when Core 0 tries to write to flash during pairing
5. This interference causes the SSP authentication process to fail

### Why It Worked on Pico W But Not Pico 2 W

The issue likely existed on both platforms, but Pico 2 W (RP2350) may have:
- Different flash access timing
- More strict flash access requirements
- Different CYW43 driver behavior
- Timing differences that made the conflict more likely

## Solution

Added `flash_safe_execute_core_init()` to Core 1's entry function to coordinate flash access between cores.

### Implementation

**File**: `src/platform/amiga/quad_mouse.c`

1. **Added include** (line 23):
   ```c
   #include "pico/flash.h"  // For flash_safe_execute_core_init() - required for Bluetooth flash coordination
   ```

2. **Added flash-safe initialization** at the start of `amiga_quad_mouse_motion()`:
   ```c
   void amiga_quad_mouse_motion()
   {
       // CRITICAL: Initialize flash-safe execution FIRST
       // This allows Core 0 to coordinate with Core 1 when Bluetooth writes to flash (TLV storage)
       // Without this, Core 1 can freeze when Bluetooth tries to access flash during pairing
       // This is required for proper flash coordination, especially for devices requiring SSP (Secure Simple Pairing)
       // Reference: Atari keyboard interface implementation
       flash_safe_execute_core_init();
       
       // ... rest of mouse processing code
   }
   ```

### What This Does

- **Initializes flash-safe execution** on Core 1
- **Allows Core 0 to coordinate** with Core 1 when writing to flash
- **Prevents Core 1 from freezing** during flash writes
- **Enables proper SSP pairing** for devices like DS5 that require flash writes during authentication

## Reference Implementation

This fix was based on the Atari keyboard adapter implementation:
- **Location**: `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultramegausb-atari-st-rpikbd/src/main.cpp`
- **Line 179**: `flash_safe_execute_core_init();` in `core1_entry()`
- **Comment**: "CRITICAL: Initialize flash-safe execution FIRST. This allows Core 0 to coordinate with Core 1 when Bluetooth writes to flash (TLV storage). Without this, Core 1 can freeze when Bluetooth tries to access flash."

## Testing

After implementing this fix:
- DS5 successfully pairs on Pico 2 W
- SSP authentication completes successfully
- Link keys are properly stored in flash
- Device remains connected and functional

## Important Notes for Future Adapter Builds

### When to Include This Fix

**Always include `flash_safe_execute_core_init()` if:**
- Using Core 1 for any processing (mouse, keyboard, etc.)
- Bluetooth is enabled and uses TLV storage for pairing persistence
- Supporting devices that use SSP (Secure Simple Pairing)
- Building for Pico 2 W (RP2350) or Pico W (RP2040)

### Where to Add It

- **Must be called in Core 1's entry function** (the function launched with `multicore_launch_core1()`)
- **Must be called FIRST**, before any other Core 1 initialization
- **Only needs to be called once** at Core 1 startup

### Related Code

The fix works in conjunction with:
- **Core 1 pause/resume functions** (`amiga_quad_mouse_pause_core1()` / `amiga_quad_mouse_resume_core1()`) - these pause Core 1 during Bluetooth enumeration
- **Bluetooth TLV storage** - uses flash to store pairing keys
- **BTstack configuration** - configured for flash-based TLV storage

## Additional Context

### Devices Affected

This fix is particularly important for:
- **DualSense 5 (DS5)** - Uses SSP with numeric confirmation
- **Other PlayStation controllers** - May use similar pairing mechanisms
- **Any device requiring SSP** - Secure Simple Pairing requires flash writes

### Devices Not Affected

- **Stadia controller** - Works without this fix (uses different pairing mechanism)
- **Simple keyboards/mice** - May work without flash coordination (less strict timing)

### Branch Information

- **Fixed in**: `gpio-ordering` branch
- **Also needed in**: Any branch with Bluetooth support and Core 1 processing
- **Date**: January 2025

## Troubleshooting

If DS5 pairing still fails after this fix:

1. **Verify flash-safe init is called**: Check that `flash_safe_execute_core_init()` is in Core 1's entry function
2. **Check Core 1 pause timing**: Ensure Core 1 is paused during gamepad discovery/connection
3. **Verify TLV storage**: Check that Bluetooth TLV storage is properly configured
4. **Check serial output**: Look for SSP confirmation requests in the logs
5. **Compare with working build**: Ensure all flash coordination code matches the Atari implementation

## Summary

**Problem**: DS5 pairing failed on Pico 2 W due to flash access conflicts between Core 0 (Bluetooth) and Core 1 (mouse processing).

**Solution**: Added `flash_safe_execute_core_init()` to Core 1's entry function to coordinate flash access.

**Result**: DS5 now pairs successfully on Pico 2 W, with proper SSP authentication and flash-based key storage.

**Key Takeaway**: Always initialize flash-safe execution in Core 1 when using Bluetooth with TLV storage and Core 1 processing.

