# GPIO Fix and Watchdog Implementation Summary

## Overview
This document summarizes the changes made to address intermittent mouse/joystick failures by implementing GPIO state management and a lightweight watchdog system.

## Problem Statement
Intermittent failures were observed where mouse and joystick inputs would stop working. Analysis suggested potential causes:
- Power sequencing issues (Amiga pull-ups pulling lines high before Pico initializes)
- GPIO state corruption during operation
- Race conditions during initialization

## Solution: Two-Part Approach

### 1. GPIO Clearing on Power-Up

**Location**: `src/main.c` (lines 84-94)

**Implementation**:
- Added a 100ms delay after `amiga_init()` to allow power to stabilize
- Added call to `amiga_gpio_reset_all_to_input()` before initializing mouse/joystick ports
- This ensures all Amiga-related GPIOs are set to a known inactive (INPUT) state
- Only active for Revision 5 hardware (`#if HIDPICO_REVISION == 5`)

**Code**:
```c
#if HIDPICO_REVISION == 5
    // Wait for power to stabilize (especially important if Amiga is already powered)
    // This prevents race conditions where Amiga's pull-ups pull lines high before Pico initializes
    sleep_ms(100);
    
    // Clear all GPIO direction cache and reset all Amiga GPIOs to INPUT (inactive/high) state
    // This ensures clean state even if Amiga is already powered and pull-ups are active
    amiga_gpio_reset_all_to_input();
    
    printf("GPIO state cleared and reset to INPUT\n");
#endif
```

### 2. Lightweight Watchdog

**Location**: 
- `src/main.c` (lines 110-141) - Watchdog check in main loop
- `src/platform/common/gpio_util.c` (lines 110-160) - Watchdog implementation
- `src/platform/common/gpio_util.h` (lines 59-64) - Watchdog declaration

**Implementation**:
- Periodic check every 5 seconds (configurable via `WATCHDOG_INTERVAL_MS`)
- Samples 4 representative GPIOs (one direction and one button from each port)
- Compares actual hardware GPIO direction with cached direction state
- If mismatch detected, triggers full GPIO reset to recover
- Minimal performance impact: only checks 4 GPIOs every 5 seconds

**Code**:
```c
// In main loop:
absolute_time_t last_watchdog_check = get_absolute_time();
const uint32_t WATCHDOG_INTERVAL_MS = 5000;  // Check every 5 seconds

// In main loop:
#if HIDPICO_REVISION == 5
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(last_watchdog_check, now) >= (WATCHDOG_INTERVAL_MS * 1000)) {
        if (amiga_gpio_watchdog_check()) {
            printf("[WATCHDOG] GPIO state recovery performed\n");
        }
        last_watchdog_check = now;
    }
#endif
```

**Watchdog Function**:
```c
bool amiga_gpio_watchdog_check(void)
{
    // Sample GPIOs to check (one from each port)
    const uint32_t sample_gpios[] = {
        QM1_AMIGA_V,   // Port 1 UP direction
        QM1_AMIGA_B1,  // Port 1 Fire button
        QM2_AMIGA_V,   // Port 2 UP direction
        QM2_AMIGA_B1,  // Port 2 Fire button
    };
    
    // Check if any sampled GPIO has mismatched direction
    // If mismatch found, reset all GPIOs to INPUT state
    // Returns true if recovery was performed
}
```

## New Functions Added

### `amiga_gpio_clear_all_cache()`
- Clears all cached GPIO direction states
- Called before full GPIO reset

### `amiga_gpio_reset_all_to_input()`
- Resets all Amiga joystick/mouse GPIOs to INPUT (inactive/high) state
- Initializes all 14 GPIOs (7 per port: H, V, HQ, VQ, B1, B2, B3)
- Clears GPIO direction cache before resetting

### `amiga_gpio_watchdog_check()`
- Lightweight watchdog that samples GPIOs to detect stuck states
- Returns `true` if recovery was performed, `false` otherwise
- Only active for Revision 5 hardware

## Performance Impact

**GPIO Clearing on Power-Up**:
- One-time cost: ~100ms delay + GPIO initialization
- Negligible impact on normal operation

**Watchdog**:
- Checks 4 GPIOs every 5 seconds
- Estimated overhead: <0.01% CPU time
- No impact on normal operation performance

## GPIOs Affected

**Port 1 / Mouse** (7 GPIOs):
- `QM1_AMIGA_H` - Horizontal direction
- `QM1_AMIGA_V` - Vertical direction
- `QM1_AMIGA_HQ` - Horizontal quadrature
- `QM1_AMIGA_VQ` - Vertical quadrature
- `QM1_AMIGA_B1` - Fire button
- `QM1_AMIGA_B2` - Button 2
- `QM1_AMIGA_B3` - Button 3

**Port 2** (7 GPIOs):
- `QM2_AMIGA_H` - Horizontal direction
- `QM2_AMIGA_V` - Vertical direction
- `QM2_AMIGA_HQ` - Horizontal quadrature
- `QM2_AMIGA_VQ` - Vertical quadrature
- `QM2_AMIGA_B1` - Fire button
- `QM2_AMIGA_B2` - Button 2
- `QM2_AMIGA_B3` - Button 3

## Testing Recommendations

1. **Power Sequencing Test**: Power on Amiga first, then Pico, verify mouse/joystick work
2. **Watchdog Test**: Monitor serial output for `[WATCHDOG]` messages during extended operation
3. **Stress Test**: Run for extended periods to verify no intermittent failures
4. **Recovery Test**: If watchdog triggers, verify mouse/joystick continue working after recovery

## Files Modified

1. `src/main.c` - Added GPIO clearing and watchdog check
2. `src/platform/common/gpio_util.c` - Added reset and watchdog functions
3. `src/platform/common/gpio_util.h` - Added function declarations

## Build Information

- **Target Hardware**: Revision 5 (Pico W / Pico 2 W)
- **Conditional Compilation**: All changes are wrapped in `#if HIDPICO_REVISION == 5`
- **Backward Compatibility**: No changes to other hardware revisions

## Date
Implementation completed: [Current Date]

