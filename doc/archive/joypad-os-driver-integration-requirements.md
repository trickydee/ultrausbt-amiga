# Joypad OS Driver Integration Requirements

**Date**: 2025-01-XX  
**Branch**: `joystickos-investigation`

## Answer: Each Driver Requires Integration

**Short answer**: Each driver requires manual integration, and they all depend on Joypad OS's core architecture. You cannot just "enable all drivers" - you must either:

1. **Adopt Joypad OS's full architecture** (router, input_event, player management, feedback)
2. **Port each driver individually** to work with AmigaHID-Pico's architecture (significant work per driver)

## How Joypad OS's Driver System Works

### Driver Registration (Manual)

Each driver must be manually included and registered in `hid_registry.c`:

```c
// 1. Include the driver header
#include "devices/vendors/sony/sony_ds4.h"

// 2. Register it in the registry
void register_devices() {
    device_interfaces[CONTROLLER_DUALSHOCK4] = &sony_ds4_interface;
    // ... repeat for each driver
}
```

**You can selectively include/exclude drivers** - you don't need all of them.

### Driver Interface (Standard)

Each driver implements a `DeviceInterface` struct:

```c
typedef struct {
    const char* name;
    
    // Device identification
    bool (*is_device)(uint16_t vid, uint16_t pid);
    bool (*check_descriptor)(uint8_t dev_addr, uint8_t instance, 
                             uint8_t const* desc_report, uint16_t desc_len);
    
    // Input processing
    void (*process)(uint8_t dev_addr, uint8_t instance, 
                    const uint8_t *report, uint16_t len);
    
    // Output/feedback task (rumble, LEDs)
    void (*task)(uint8_t dev_addr, uint8_t instance, 
                 device_output_config_t* config);
    
    // Lifecycle
    bool (*init)(uint8_t dev_addr, uint8_t instance);
    void (*unmount)(uint8_t dev_addr, uint8_t instance);
    
    // Device capabilities
    uint16_t (*get_capabilities)(void);
} DeviceInterface;
```

## Critical Dependencies

### All Drivers Depend On:

1. **Router System** (`core/router/router.h`)
   - Drivers call `router_submit_input(&event)` to submit input
   - This is Joypad OS's input→output routing system
   - **AmigaHID-Pico doesn't have this** - we directly control GPIO

2. **Input Event Structure** (`core/input_event.h`)
   - Drivers create `input_event_t` structures
   - Standardized format for all input types
   - **AmigaHID-Pico uses ad-hoc structures**

3. **Player Management** (`core/services/players/manager.h`)
   - Tracks which device is assigned to which player
   - **AmigaHID-Pico has fixed mapping** (gamepad 0 → port 2, gamepad 1 → port 1)

4. **Feedback System** (`core/services/players/feedback.h`)
   - Rumble, LED, adaptive trigger management
   - **AmigaHID-Pico doesn't use rumble/LEDs** (Amiga doesn't support)

5. **Button Definitions** (`core/buttons.h`)
   - W3C Gamepad API button order
   - **AmigaHID-Pico uses different button mapping**

### Example Driver Dependencies

Looking at `sony_ds4.c`:

```c
#include "sony_ds4.h"
#include "core/buttons.h"              // Button definitions
#include "core/router/router.h"        // Router system
#include "core/input_event.h"          // Input event structure
#include "core/services/players/manager.h"  // Player management
#include "core/services/players/feedback.h" // Rumble/LED feedback
```

The driver processes input and calls:
```c
input_event_t event = { /* ... */ };
router_submit_input(&event);  // Submit to router system
```

## Integration Options

### Option 1: Full Architecture Adoption (Recommended if adopting)

**Pros:**
- All drivers work immediately
- Clean, modular architecture
- Easy to add new drivers
- Router system enables advanced features (input merging, broadcasting)

**Cons:**
- Major architectural change
- Requires porting AmigaHID-Pico's GPIO control to output interface
- Significant refactoring effort
- May be overkill for AmigaHID-Pico's simpler use case

**Effort**: High (weeks of work)

### Option 2: Port Individual Drivers (Selective)

**Pros:**
- Can pick and choose which drivers to support
- Minimal architectural changes
- Focus on high-value drivers (DS3/DS4/DualSense, Switch Pro)

**Cons:**
- Each driver requires porting work
- Must adapt from `router_submit_input()` to direct GPIO control
- Must adapt from `input_event_t` to AmigaHID-Pico's structures
- Must remove dependencies on player management, feedback system
- Duplicate work for each driver

**Effort**: Medium per driver (days of work each)

### Option 3: Hybrid Approach (Pragmatic)

**Pros:**
- Adopt `input_event_t` structure (low effort, high value)
- Port only high-value drivers (DS3/DS4/DualSense, Switch Pro)
- Keep AmigaHID-Pico's direct GPIO control
- Minimal architectural changes

**Cons:**
- Still requires porting work per driver
- Can't use drivers "as-is"

**Effort**: Medium (days to weeks, depending on how many drivers)

## Recommended Approach

### Phase 1: Adopt Input Event Structure (Low Risk, High Value)

1. **Adopt `input_event_t` structure**:
   - Replace ad-hoc gamepad structures
   - Normalize analog values (0-255, centered at 128)
   - Standard button bitmap

2. **Create adapter functions**:
   - Convert `input_event_t` → Amiga GPIO control
   - Keep existing GPIO control code

**Effort**: Low-Medium (days)

### Phase 2: Port High-Value Drivers (Selective)

Port only the most valuable drivers:
1. **DualShock 3/4/DualSense** - Better button mapping, motion, touchpad
2. **Switch Pro** - Capture/home buttons, motion
3. **Generic HID gamepad** - Already works, but could improve parsing

For each driver:
1. Copy driver code
2. Replace `router_submit_input()` with direct GPIO control
3. Remove dependencies on player management, feedback system
4. Adapt to AmigaHID-Pico's button mapping

**Effort**: Medium per driver (2-3 days each)

### Phase 3: Consider Full Architecture (If Needed)

Only if you need advanced features:
- Multiple gamepads → single port (input merging)
- One gamepad → multiple ports (broadcasting)
- Input transformations (mouse→analog, spinner accumulation)

**Effort**: High (weeks)

## What You Get "For Free"

**Nothing runs automatically** - you must:

1. ✅ **Include driver headers** (easy)
2. ✅ **Register drivers** (easy)
3. ❌ **Port to your architecture** (required for each driver)
4. ❌ **Remove dependencies** (required for each driver)

## Code Example: Porting a Driver

### Original (Joypad OS):
```c
void input_sony_ds4(uint8_t dev_addr, uint8_t instance, 
                    uint8_t const* report, uint16_t len) {
    // Parse DS4 report
    sony_ds4_report_t* ds4 = (sony_ds4_report_t*)report;
    
    // Create input event
    input_event_t event = {
        .buttons = /* map buttons */,
        .analog[ANALOG_LX] = ds4->x,
        .analog[ANALOG_LY] = ds4->y,
        // ...
    };
    
    // Submit to router
    router_submit_input(&event);
}
```

### Ported (AmigaHID-Pico):
```c
void input_sony_ds4(uint8_t dev_addr, uint8_t instance, 
                    uint8_t const* report, uint16_t len) {
    // Parse DS4 report (same)
    sony_ds4_report_t* ds4 = (sony_ds4_report_t*)report;
    
    // Convert to directions/buttons
    bool up = (ds4->dpad == 0 || ds4->dpad == 7 || ds4->dpad == 1);
    bool down = (ds4->dpad == 4 || ds4->dpad == 3 || ds4->dpad == 5);
    // ... map all directions and buttons
    
    // Direct GPIO control (instead of router)
    if (dev_addr == first_gamepad_dev_addr) {
        amiga_joystick_port2_set_direction(AJ2_UP, up);
        amiga_joystick_port2_set_direction(AJ2_DOWN, down);
        // ...
        amiga_joystick_port2_set_button(AJ2_FIRE, ds4->cross);
        // ...
    }
}
```

## Conclusion

**Each driver requires integration work**. You cannot just "enable all drivers" - they all depend on Joypad OS's core architecture (router, input_event, player management, feedback).

**Recommended approach**:
1. Adopt `input_event_t` structure (low effort, high value)
2. Port only high-value drivers (DS3/DS4/DualSense, Switch Pro)
3. Keep AmigaHID-Pico's direct GPIO control (simpler, fits use case)

**Estimated effort**:
- Adopt `input_event_t`: 2-3 days
- Port DS3/DS4/DualSense: 3-5 days
- Port Switch Pro: 2-3 days
- **Total for high-value drivers**: ~1-2 weeks

This gives you the benefits of better device compatibility without the overhead of the full router architecture.


