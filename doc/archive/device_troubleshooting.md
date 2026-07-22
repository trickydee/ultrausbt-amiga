# Device Troubleshooting Guide

This document covers troubleshooting fixes for various device compatibility issues encountered during development.

## Table of Contents

1. [DS5 DualSense Pairing Fix for Pico 2 W](#ds5-dualsense-pairing-fix-for-pico-2-w)
2. [Stadia Controller Bluetooth Pairing Issues](#stadia-controller-bluetooth-pairing-issues)
3. [Joystick Port 1 LEFT/RIGHT Movement Fix](#joystick-port-1-leftright-movement-fix)
4. [Keyboard HID debug and custom key remaps](#keyboard-hid-debug-and-custom-key-remaps)
5. [Keyboard reset combos (classic and alternate)](#keyboard-reset-combos-classic-and-alternate)

---

## DS5 DualSense Pairing Fix for Pico 2 W

### Problem Summary

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

### Root Cause

The issue was **missing flash-safe execution coordination** between Core 0 (Bluetooth) and Core 1 (mouse processing).

#### Why Flash Coordination Matters

1. **DS5 uses SSP (Secure Simple Pairing)** which requires writing link keys to flash memory during pairing
2. **Bluetooth TLV storage** uses flash to persist pairing keys across power cycles
3. **Core 1 (mouse processing)** runs independently and can access flash simultaneously
4. **Without coordination**, Core 1 can freeze or interfere when Core 0 tries to write to flash during pairing
5. This interference causes the SSP authentication process to fail

#### Why It Worked on Pico W But Not Pico 2 W

The issue likely existed on both platforms, but Pico 2 W (RP2350) may have:
- Different flash access timing
- More strict flash access requirements
- Different CYW43 driver behavior
- Timing differences that made the conflict more likely

### Solution

Added `flash_safe_execute_core_init()` to Core 1's entry function to coordinate flash access between cores.

#### Implementation

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

### Reference Implementation

This fix was based on the Atari keyboard adapter implementation:
- **Location**: `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultramegausb-atari-st-rpikbd/src/main.cpp`
- **Line 179**: `flash_safe_execute_core_init();` in `core1_entry()`
- **Comment**: "CRITICAL: Initialize flash-safe execution FIRST. This allows Core 0 to coordinate with Core 1 when Bluetooth writes to flash (TLV storage). Without this, Core 1 can freeze when Bluetooth tries to access flash."

### Testing

After implementing this fix:
- DS5 successfully pairs on Pico 2 W
- SSP authentication completes successfully
- Link keys are properly stored in flash
- Device remains connected and functional

### Important Notes for Future Adapter Builds

#### When to Include This Fix

**Always include `flash_safe_execute_core_init()` if:**
- Using Core 1 for any processing (mouse, keyboard, etc.)
- Bluetooth is enabled and uses TLV storage for pairing persistence
- Supporting devices that use SSP (Secure Simple Pairing)
- Building for Pico 2 W (RP2350) or Pico W (RP2040)

#### Where to Add It

- **Must be called in Core 1's entry function** (the function launched with `multicore_launch_core1()`)
- **Must be called FIRST**, before any other Core 1 initialization
- **Only needs to be called once** at Core 1 startup

#### Related Code

The fix works in conjunction with:
- **Core 1 pause/resume functions** (`amiga_quad_mouse_pause_core1()` / `amiga_quad_mouse_resume_core1()`) - these pause Core 1 during Bluetooth enumeration
- **Bluetooth TLV storage** - uses flash to store pairing keys
- **BTstack configuration** - configured for flash-based TLV storage

### Additional Context

#### Devices Affected

This fix is particularly important for:
- **DualSense 5 (DS5)** - Uses SSP with numeric confirmation
- **Other PlayStation controllers** - May use similar pairing mechanisms
- **Any device requiring SSP** - Secure Simple Pairing requires flash writes

#### Devices Not Affected (by the DS5 SSP flash-init issue alone)

- **Simple keyboards/mice** - Often work without extra SSP coordination (less strict timing)
- **Stadia** - Still needs Core 1 / flash cooperation; see the Stadia section below (different failure mode than DS5 SSP)

---

## Stadia Controller Bluetooth Pairing Issues

**Canonical write-up:** [`stadia-controller-verification.md`](./stadia-controller-verification.md) § *BLE pairing + mouse motion*.  
**Family handoff:** [`archive/BT_PAIRING_HANDOFF.md`](./archive/BT_PAIRING_HANDOFF.md).

### Problem Summary

Stadia BLE pairing has gone through several failure modes on this dual-core Pico 2 W adapter:

1. **Early:** full hangs during GATT / bond (flash vs Core 1 XIP).
2. **Mid:** pause/refcount mistakes (Core 1 stuck paused after failed reconnect).
3. **July 2026 (v2.2.13–v2.2.17):** Stadia pairs and Port 2 works; **mouse buttons work**; **cursor motion dead**. Core 1 heartbeat still climbing.

### Latest smoking gun (v2.2.16–v2.2.17 DIAG)

```text
motion_feeds↑  consumed=0  quad_gpio=0  flag=1  hb↑  paused=0  port1=MOUSE
```

Core 0 was posting motion; Core 1 never consumed it or drove quadrature GPIOs.

### Root Cause (confirmed)

Motion consume + quad GPIO updates in `amiga_quad_mouse_motion()` were gated on `absolute_time_diff_us(...) >= update_period_us`. After Stadia bond / `flash_safe_execute` lockout, that **time gate stopped opening** while the Core 1 loop kept running. Stadia is a strong trigger because its bond/re-encrypt path does more TLV flash work than a typical mouse pair.

**Not** the cause of the `consumed=0` failure:

- Rev 6 GPIO pin remap (orthogonal; Rev 5 builds failed the same way).
- USB Stadia report-format parsing (BLE path is Bluepad32).
- Memory barriers alone (hygiene; DIAG showed the timed block never ran).

Earlier pause/resume / `flash_safe_execute_core_init()` work remains necessary for hang avoidance, but was **not sufficient** for this motion-dead mode.

### Fix (v2.2.18)

**File:** `src/platform/amiga/quad_mouse.c`

- Period from a **loop counter**, not `get_absolute_time()`.
- Consume `motion_flag` as soon as it is set; keep period ticks for the quadrature state machine.
- `__dmb()` on the Core0↔Core1 motion handoff.
- Prefer `busy_wait_us` over `sleep_us` on Core 1 after BT flash activity.

Healthy DIAG: `consumed` tracks `motion_feeds`, `quad_gpio` rises, `period` climbs, `flag=0` at idle.

### Supporting mitigations (still relevant)

These reduce hangs / pause stacking; they do not replace the v2.2.18 consume-path fix:

| Mitigation | Role |
|------------|------|
| `flash_safe_execute_core_init()` on Core 1 entry | Required for TLV/bond flash |
| Refcounted BT pause + force-release / watchdog | Avoid stuck `paused` after failed reconnect |
| Optional skip of discovery pause for some gamepads | A/B; not the `consumed=0` fix |
| Heartbeat stall → SEV / relaunch | Recovers brief Core 1 park during bond |
| Defer heavy OLED work during enumerate | Reduces interference; not root cause |

### Devices / notes

- **Stadia** (`0x18D1` / `0x9400`) — strongest trigger seen for motion-dead after pair.
- **Xbox Wireless** — same general BT/flash class; retest after Core 1 changes.
- Brief `[BT] Core 1 heartbeat stalled … relaunching` during Stadia setup can still appear; mouse should recover after v2.2.18.

---

## Joystick Port 1 LEFT/RIGHT Movement Fix

### Problem Summary

Joystick Port 1 LEFT and RIGHT directions were not working when in joystick mode. UP, DOWN, and FIRE worked correctly, but LEFT/RIGHT had no effect. Additionally, pressing Button 2 (Circle) and Button 3 (Square) on gamepads was triggering LEFT/RIGHT movements instead of button presses.

### Symptoms

1. **LEFT/RIGHT not working**: Joystick stick LEFT/RIGHT had no effect on Amiga
2. **Button confusion**: Circle button (Button 2) triggered LEFT movement
3. **Button confusion**: Square button (Button 3) triggered RIGHT movement
4. **Intermittent behavior**: Sometimes LEFT/RIGHT would work briefly, then stop

### Root Cause

Multiple issues were causing the problem:

#### Issue 1: GPIO Direction Cache Problem

The GPIO direction cache was preventing correct direction setting:
- `amiga_gpio_set_active_low()` only set GPIO direction if cache indicated it needed changing
- Mouse code on Core 1 could change GPIO direction
- Cache would be out of sync with actual hardware state
- Joystick code would skip setting direction, leaving GPIO in wrong state

#### Issue 2: Cross-Core Race Condition

Mouse quadrature code (Core 1) was overwriting joystick GPIOs (Core 0):
- Core 0 sets LEFT/RIGHT GPIO to OUTPUT and LOW
- Core 1 checks joystick mode (may read stale value due to cache)
- Core 1 updates GPIO, overwriting Core 0's setting
- Result: LEFT/RIGHT direction lost

#### Issue 3: Button GPIO Conflicts

Button 2 and Button 3 shared GPIOs with LEFT/RIGHT directions:
- `QM1_AMIGA_B2 = GPIO 12` (same as LEFT direction `QM1_AMIGA_VQ`)
- `QM1_AMIGA_B3 = GPIO 13` (same as RIGHT direction `QM1_AMIGA_HQ`)
- Pressing Button 2 set GPIO 12 LOW = LEFT direction active
- Pressing Button 3 set GPIO 13 LOW = RIGHT direction active

### Solution

Implemented a multi-part fix addressing all three issues.

#### Fix 1: Always Set GPIO Direction

**File**: `src/platform/common/gpio_util.c`

```c
void amiga_gpio_set_active_low(uint32_t gpio, bool active)
{
    if (active) {
        // Always set direction to OUTPUT FIRST, then set level
        // This ensures the GPIO is in the correct state even if mouse code changed it
        gpio_set_dir(gpio, GPIO_OUT);
        __sync_synchronize();  // Memory barrier for cross-core visibility
        gpio_put(gpio, 0);
        gpio_dir_cache |= (1U << gpio);
    } else {
        // Always set direction to INPUT FIRST
        gpio_set_dir(gpio, GPIO_IN);
        __sync_synchronize();  // Memory barrier for cross-core visibility
        gpio_dir_cache &= ~(1U << gpio);
    }
}
```

**Key Change**: Always set GPIO direction regardless of cache state.

#### Fix 2: Memory Barriers for Cross-Core Synchronization

**File**: `src/platform/amiga/joystick_port1.c`

```c
if (dir_left != prev_dir_left) {
    // Use memory barrier before setting GPIO to ensure Core 1 sees joystick mode change
    __sync_synchronize();
    amiga_gpio_set_active_low(QM1_AMIGA_VQ, dir_left);  // VQ pin = LEFT (GPIO 12)
    __sync_synchronize();
    prev_dir_left = dir_left;
}
```

**File**: `src/platform/amiga/quad_mouse.c`

```c
// Use memory barrier to ensure we see the latest joystick mode state
__sync_synchronize();
bool joy_mode = amiga_joystick_port1_is_joystick_mode();
__sync_synchronize();
if (!joy_mode) {
    // Only update GPIOs if not in joystick mode
    // ...
}
```

**Key Change**: Added memory barriers to ensure Core 1 sees joystick mode changes before GPIO updates.

#### Fix 3: Disable Button 2/3 in Joystick Mode

**File**: `src/platform/amiga/joystick_port1.c`

```c
void amiga_joystick_port1_set_button(enum amiga_joystick_port1_buttons button, bool pressed)
{
    switch (button) {
        case AJ1_FIRE:
            amiga_gpio_set_active_low(QM1_AMIGA_B1, pressed);
            break;
        case AJ1_BUTTON2:
            // Button 2 uses GPIO 12, which is the same as LEFT direction (QM1_AMIGA_VQ)
            // In joystick mode, we can't use Button 2 because it conflicts with LEFT direction
            // Only set Button 2 if NOT in joystick mode (when port 1 is in mouse mode)
            if (!port1_joystick_mode) {
                amiga_gpio_set_active_low(QM1_AMIGA_B2, pressed);
            }
            // Otherwise, ignore Button 2 to prevent conflict with LEFT direction
            break;
        case AJ1_BUTTON3:
            // Button 3 uses GPIO 13, which is the same as RIGHT direction (QM1_AMIGA_HQ)
            // In joystick mode, we can't use Button 3 because it conflicts with RIGHT direction
            // Only set Button 3 if NOT in joystick mode (when port 1 is in mouse mode)
            if (!port1_joystick_mode) {
                amiga_gpio_set_active_low(QM1_AMIGA_B3, pressed);
            }
            // Otherwise, ignore Button 3 to prevent conflict with RIGHT direction
            break;
    }
}
```

**Key Change**: Disable Button 2/3 when joystick mode is active to prevent GPIO conflicts.

### GPIO Pin Mapping (Revision 5)

For reference, the GPIO pin assignments are:
- **UP**: GPIO 10 (`QM1_AMIGA_V`)
- **DOWN**: GPIO 11 (`QM1_AMIGA_H`)
- **LEFT**: GPIO 12 (`QM1_AMIGA_VQ`) - **CONFLICTS with Button 2**
- **RIGHT**: GPIO 13 (`QM1_AMIGA_HQ`) - **CONFLICTS with Button 3**
- **FIRE**: GPIO 14 (`QM1_AMIGA_B1`)
- **Button 2**: GPIO 12 - **SAME as LEFT** (disabled in joystick mode)
- **Button 3**: GPIO 13 - **SAME as RIGHT** (disabled in joystick mode)

### Testing

After implementing these fixes:
- LEFT/RIGHT joystick movement works correctly
- Button 2/3 no longer trigger LEFT/RIGHT movements
- No more intermittent behavior
- All directions (UP, DOWN, LEFT, RIGHT) work consistently

### Important Notes

#### Hardware Limitation

Button 2 and Button 3 are currently disabled in joystick mode due to GPIO conflicts. The config comments note: "will be remapped in hardware" - this is a known limitation that requires hardware changes to fully resolve.

#### Memory Barrier Importance

The `__sync_synchronize()` calls are critical for cross-core synchronization:
- Without them, Core 1 may not see joystick mode changes
- This causes mouse code to overwrite joystick GPIOs
- Results in LEFT/RIGHT not working

#### GPIO Direction Setting

Always setting GPIO direction (regardless of cache) ensures:
- GPIO is in correct state even if mouse code changed it
- No dependency on cache state
- Reliable operation across core boundaries

### Future Improvements

1. **Hardware Remapping**: Remap Button 2/3 to different GPIOs to eliminate conflicts
2. **GPIO Arbitration**: Implement proper GPIO ownership tracking between cores
3. **Cache Invalidation**: Clear GPIO cache when switching between mouse/joystick modes

---

## Button 2/3 Voltage Levels - Amiga Model Compatibility

### Problem Summary

Buttons 2 and 3 on both joystick ports show higher voltage levels (400-500mV) when pressed compared to other buttons (50-80mV). This causes buttons 2 and 3 to not work on some Amiga models, while working correctly on others.

### Symptoms

1. **Voltage Measurements**:
   - Normal buttons (UP/DOWN/LEFT/RIGHT/FIRE): 5V → 50-80mV when pressed
   - Buttons 2 and 3: 5V → 400-500mV when pressed
   
2. **Amiga Model Differences**:
   - **A2000**: Buttons 2 and 3 work correctly with 500mV
   - **Other Amiga models** (A500, A600, A1200, etc.): Buttons 2 and 3 don't work with 500mV

3. **Consistent Across Hardware**:
   - Same voltage levels on different Pico boards (RP2040, RP2350)
   - Voltage levels tested on both GPIO 27 (current) and GPIO 7 (temporarily used during debugging)
   - LED test shows GPIOs are working correctly

### Root Cause

This is **NOT a firmware or Pico issue** - it's an **Amiga model compatibility difference**:

1. **Different Pull-up Resistor Values**: Different Amiga models use different pull-up resistor values on joystick port pins
2. **Different Input Buffer Thresholds**: Different Amiga models have different voltage thresholds for recognizing LOW signals
3. **Different Input Impedance**: Different loading characteristics on different Amiga models

The firmware is working correctly - it's driving the GPIOs LOW properly. The voltage difference is due to:
- **Amiga's internal pull-up resistors** pulling the line higher
- **Level shifter characteristics** (if using level shifters)
- **Amiga input buffer characteristics** varying by model

### Why A2000 Works But Others Don't

The A2000 likely has:
- **Stronger pull-up resistors** that are easier to overcome
- **More tolerant input buffers** that accept higher voltages as LOW
- **Different input impedance** that allows the Pico to pull the line lower

Other Amiga models (A500, A600, A1200) likely have:
- **Stricter voltage thresholds** requiring <100mV for LOW
- **Different pull-up values** that create the 400-500mV level
- **More sensitive input buffers** that don't recognize 500mV as LOW

### Solution

This is a **hardware compatibility limitation**, not a firmware bug. The firmware is working correctly.

#### Options for Affected Amiga Models

**Important Note**: The issue is NOT about pull-down resistors. The Amiga's pull-ups are pulling HIGH, and we need the level shifter to SINK more current when driving LOW.

1. **Hardware Solution**: Use a level shifter with higher current sinking capability
   - The TXB0108 can sink ~24mA per channel
   - Some Amiga models may have stronger pull-ups that require more current to overcome
   - Consider using a level shifter with higher current rating (e.g., SN74LVC8T245 can sink 32mA)

2. **Hardware Solution**: Use a different level shifter design (e.g., BSS138 MOSFETs instead of TXB0108)
   - The original amigahid-pico design uses BSS138 MOSFETs with 10kΩ pull-ups on both sides
   - This design has proven reliable and works with all Amiga models
   - BSS138 MOSFETs can sink more current than TXB0108

3. **Hardware Solution**: Add buffer/driver ICs to increase drive strength on buttons 2 and 3
   - Use a dedicated buffer IC (e.g., 74LVC1G07) that can sink more current
   - Place buffer between level shifter and Amiga for buttons 2/3 only
   - This increases current sinking capability without affecting other signals

4. **Hardware Solution**: Use stronger pull-up resistors on the 3.3V side (if using TXB0108)
   - The TXB0108 requires pull-ups on BOTH sides for proper operation
   - Stronger pull-ups on the 3.3V side can help the level shifter detect direction better
   - But this doesn't directly solve the current sinking issue

5. **Accept Limitation**: Buttons 2 and 3 may not work on all Amiga models due to voltage threshold differences
   - This is a known hardware compatibility limitation
   - The firmware is working correctly
   - Some Amiga models simply have stricter voltage thresholds

#### Why Pull-Down Resistors Won't Help

**Pull-down resistors would make things WORSE**, not better:
- When signal should be HIGH (inactive): Amiga's pull-up pulls to 5V, but pull-down would fight it, creating a voltage divider
- When signal should be LOW (active): Pico drives LOW, but pull-down would add unnecessary current draw
- The real issue is that the level shifter can't SINK enough current to overcome the Amiga's pull-up when driving LOW

#### Ground Connection Issues (CRITICAL!)

**If you have grounds connected in SERIES (daisy-chained) across multiple level shifters, this is likely contributing to the problem!**

**Why Series Grounds Cause Issues:**

1. **Voltage Drop Across Ground Resistance**:
   - Each connection in a series ground path adds resistance (wire resistance, connector resistance, etc.)
   - When the level shifter tries to sink current to drive LOW, current flows: `Amiga → Level Shifter → Ground Path → Pico GND`
   - If the ground path has resistance (R), the voltage drop is: `V_drop = I_sink × R`
   - This voltage drop prevents the signal from going fully LOW
   - **Example**: If ground path has 1Ω resistance and level shifter sinks 20mA, voltage drop = 20mV
   - With multiple level shifters in series, resistance adds up, voltage drop increases

2. **Current Return Path Problems**:
   - When multiple signals try to sink current simultaneously, they all share the same series ground path
   - This increases the current through the ground path, increasing voltage drop
   - **Buttons 2/3 might be affected more** if they're at the end of the series chain

3. **Ground Loop and Noise**:
   - Series grounds can create ground loops
   - Different ground potentials at different points in the chain
   - Can cause noise and signal integrity issues

**Solution: Star Grounding**

**All grounds should connect to a COMMON POINT (star grounding), not in series:**

```
❌ BAD (Series/Daisy-Chain):
Pico GND → Level Shifter 1 GND → Level Shifter 2 GND → Level Shifter 3 GND → Amiga GND

✅ GOOD (Star Ground):
                    ┌─ Level Shifter 1 GND
                    ├─ Level Shifter 2 GND
Pico GND ──── Common Point ── Level Shifter 3 GND
                    └─ Amiga GND
```

**How to Fix:**

1. **Identify a Common Ground Point**:
   - Use a ground plane on your PCB, or
   - Use a terminal block/ground bus bar, or
   - Use a star point (single connection point where all grounds meet)

2. **Connect All Grounds to the Common Point**:
   - Pico GND → Common Point (use multiple Pico GND pins if available!)
   - Level Shifter 1 GND (3.3V side) → Common Point
   - Level Shifter 1 GND (5V side) → Common Point
   - Level Shifter 2 GND (3.3V side) → Common Point
   - Level Shifter 2 GND (5V side) → Common Point
   - Amiga GND → Common Point
   - **Each connection should be a separate wire/trace to the common point**

3. **Use Thicker Wires/Traces**:
   - Lower resistance = lower voltage drop
   - For high-current signals (like buttons 2/3), use thicker ground connections

4. **Multiple Ground Points on Pico**:
   - **YES, connecting multiple Pico GND pins to the common point WILL help!**
   - Pico has multiple GND pins - use them all!
   - This reduces resistance and provides multiple current return paths
   - Each additional GND connection reduces the overall ground path resistance

**Expected Improvement:**

- **Reduced ground resistance**: Star grounding eliminates series resistance
- **Lower voltage drop**: No voltage drop across ground path when sinking current
- **Better signal integrity**: All signals see the same ground potential
- **Buttons 2/3 should work better**: Lower ground resistance = better current sinking = lower voltage when driving LOW

**Testing:**

After switching to star grounding, measure:
- Ground path resistance (should be <0.1Ω ideally)
- Voltage at buttons 2/3 when pressed (should be closer to 50-80mV, not 400-500mV)
- Voltage drop across ground connections (should be <10mV)

#### What Actually Happens

1. **Inactive (HIGH)**: 
   - Amiga's pull-up (~10kΩ) pulls line to 5V
   - Pico sets GPIO to INPUT (high-impedance)
   - Level shifter translates 5V HIGH to 3.3V HIGH
   - ✅ Works correctly

2. **Active (LOW)**:
   - Pico drives GPIO LOW (0V) as OUTPUT
   - Level shifter should translate 3.3V LOW to 0V on 5V side
   - **Problem**: Amiga's pull-up (~10kΩ) is trying to pull to 5V
   - Level shifter must SINK current to overcome the pull-up
   - If level shifter can't sink enough current, voltage stays at 400-500mV instead of <100mV
   - ❌ Some Amiga models don't recognize 500mV as LOW

#### Firmware Status

The firmware is **working correctly**:
- GPIOs are being driven LOW properly
- LED test confirms GPIOs are working
- A2000 works perfectly with 500mV
- The issue is Amiga model-specific voltage threshold differences

### Testing Results

- **Pico GPIO Output**: Confirmed working (LED test)
- **Voltage Levels**: Consistent across different Pico boards
- **A2000 Compatibility**: Works with 500mV
- **Other Amiga Models**: May require <100mV (hardware limitation)

### Important Notes

#### This is NOT a Bug

The firmware is functioning correctly. The voltage difference is due to:
- Amiga hardware differences (pull-up values, input thresholds)
- Level shifter characteristics (if used)
- Normal variation between Amiga models

#### Known Working Models

- **A2000**: Works with 500mV (confirmed)
- **Other models**: May require hardware modifications

#### Future Hardware Improvements

For future adapter designs, consider:
- Stronger pull-down resistors on button 2/3 lines
- Different level shifter design for buttons 2/3
- Buffer/driver ICs for increased drive strength
- Separate level shifter channels for buttons 2/3 with different characteristics

---

## Keyboard HID debug and custom key remaps

Use this when a USB/Bluetooth key does not map to the Amiga key you expect (e.g. remapping a Logitech key to numpad `*` / Print Screen).

### Enable UART keystroke logging

In `src/config.h`:

```c
#define KEYBOARD_HID_DEBUG  1
```

Rebuild and flash. Connect a serial monitor to the Pico UART (115200 8N1). Each key **press** logs:

```text
[kbd] HID 0x32 -> Amiga 0x35
```

The second value is the **default** table lookup; remapped keys still send the remap target (see below).

Set back to `0` for normal use (default in release builds).

### Custom remap (one HID code → Amiga numpad *)

```c
#define KEY_REMAP_HID_TO_HELP  0x32   // HID code from debug log; 0 = disable
```

Sends **`AMIGA_KPAST` (`0x5d`)** — Amiga numpad `*` / Print Screen — when that HID code is pressed. Implemented in `amiga_hid_send()` in `keyboard_serial_io.c`. Applies to USB and Bluetooth keyboards.

---

## Keyboard reset combos (classic and alternate)

The adapter emulates the Amiga keyboard hard-reset (Ctrl-Amiga-Amiga). Two combos are recognised on USB **and** Bluetooth keyboards:

| Combo | Notes |
|-------|-------|
| **Ctrl + Left Amiga + Right Amiga** | Classic Amiga reset. Requires a keyboard with a Right Amiga / Right GUI (Right Windows / Right Command) key. |
| **Ctrl + Left Amiga + Backspace** | Alternate reset for keyboards **without** a Right Amiga key (e.g. Logitech MX Keys Mini). |

Left Amiga = Left GUI (Left Windows / Left Command).

### How the reset is signalled

The reset is asserted the way a real Amiga keyboard MCU does it: by **holding the keyboard CLOCK (`KCLK`) line LOW** (the "hard reset warning"), rather than relying only on the dedicated `/KBRST` line (which is not wired on every board). Both are driven for maximum compatibility. See `amiga_assert_reset()` / `amiga_release_reset()` in `src/platform/amiga/keyboard_serial_io.c`.

Key points implemented for reliability:

- **Minimum hold time** (`RESET_ASSERT_MIN_HOLD_MS`, default 500 ms in `config.h`). Many keyboards **ghost** — they drop a combo key from the HID report the instant a third key is added (the MX Keys Mini drops Backspace). Without a guaranteed hold, the reset pulse would be too short for the Amiga to latch. The assert blocks for this minimum on purpose (we are deliberately resetting the machine).
- **No key bit-banging while `in_reset`.** Pulsing `/clk` to send scancodes during a reset breaks the held handshake, so the transmit loop is skipped while reset is asserted (matches borb/amigahid-pico c638207, "fix held ctrl-amiga-amiga").

### Debugging the combo

Enable the reset state-machine log in `src/config.h`:

```c
#define KEYBOARD_RESET_DEBUG  1
```

Each tracked key change logs the combo state, e.g.:

```text
[reset] key=0x66 down | ctrl=1 lamiga=1 ramiga=0 bksp=1 | combo=1 in_reset=0
[reset] *** ASSERT reset ***
```

Set back to `0` for normal use (default in release builds).

---

## Summary

These fixes address critical device compatibility issues:

1. **DS5 Pairing**: Flash-safe execution coordination for SSP pairing
2. **Stadia / BLE gamepad + mouse**: Core 1 must survive bond/flash lockout; Amiga v2.2.18 stops gating quadrature consume on `absolute_time` (see [`stadia-controller-verification.md`](./stadia-controller-verification.md))
3. **Joystick LEFT/RIGHT**: GPIO direction fixes, memory barriers, and button conflict resolution
4. **Button 2/3 Voltage**: Amiga model compatibility difference (not a firmware bug)

Cross-core flash coordination and Core 1 timing that does not depend on fragile `absolute_time` gates are both required for reliable Pico 2 W + Bluepad32 operation.

