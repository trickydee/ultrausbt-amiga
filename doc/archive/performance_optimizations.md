# Performance Optimization Recommendations

This document outlines performance issues identified in the codebase and recommended optimizations to improve overall system performance and reduce unnecessary processing.

## Summary of Issues

The main performance bottlenecks are:
1. **Excessive display updates** - Display refreshes triggered on every keystroke
2. **Unnecessary Bluetooth processing** - Bluetooth functions called even when no devices are connected
3. **Always-on mouse-to-joystick conversion** - Joystick conversion runs on every mouse event
4. **No display update throttling** - Immediate display updates without debouncing

---

## 1. Display Update Optimizations

### Issue: Display Updates on Every Keystroke

**Location:** `src/platform/amiga/keyboard_serial_io.c` (lines 116, 127)

**Problem:**
- `dbgcons_amiga_key()` is called on **every** key press and release
- Each call triggers `disp_write()` → `disp_ssd_update()` → full I2C display refresh
- This can happen 100+ times per second during active typing
- I2C display updates are relatively slow (milliseconds) and block processing

**Current Code:**
```c
void amiga_hid_send(uint8_t hidcode, bool up)
{
    // ...
    dbgcons_amiga_key(hidcode, mapHidToAmiga[hidcode], up ? "u" : "d");  // ← Triggers display update
    amiga_send(mapHidToAmiga[hidcode], up);
}
```

**Impact:** High - This is the biggest performance bottleneck during keyboard use.

**Recommendations:**

#### Option A: Throttle Display Updates (Recommended)
- Add a timestamp-based throttling mechanism to limit display updates
- Update display at most once every 50-100ms
- Batch multiple key events into a single display update

#### Option B: Make Display Updates Conditional
- Add a compile-time or runtime flag to disable keyboard display updates
- Keep display updates for device plug/unplug events (less frequent)
- Optionally update display only on modifier key changes or special keys

#### Option C: Lazy Display Updates
- Mark display as "dirty" instead of immediately updating
- Update display in a low-priority background task or when main loop is idle
- Use a flag to indicate display needs refresh

**Implementation Priority:** HIGH - This will have the biggest performance impact.

---

## 2. Bluetooth Processing Optimizations

### Issue: Unnecessary Bluetooth Processing in Main Loop

**Location:** `src/main.c` (lines 76-79)

**Problem:**
- `process_bluepad32_keyboard()` and `process_bluepad32_mouse()` are called **every** iteration of the main loop
- Even when no Bluetooth devices are connected, these functions still execute
- `bluepad32_get_keyboard_count()` and `bluepad32_get_mouse_count()` iterate through arrays on every call

**Current Code:**
```c
while (1) {
    tuh_task();
    amiga_service();
    
#if ENABLE_BLUEPAD32
    bluepad32_poll();
    process_bluepad32_keyboard();  // ← Called every iteration
    process_bluepad32_mouse();     // ← Called every iteration
#endif
}
```

**Impact:** Medium - Wastes CPU cycles when no Bluetooth devices are connected.

**Recommendations:**

#### Option A: Early Return on No Devices (Recommended)
- Check device count before processing
- Skip processing functions if no devices are connected
- Cache device count state to avoid repeated array iterations

**Example:**
```c
#if ENABLE_BLUEPAD32
    bluepad32_poll();
    
    if (bluepad32_get_keyboard_count() > 0) {
        process_bluepad32_keyboard();
    }
    if (bluepad32_get_mouse_count() > 0) {
        process_bluepad32_mouse();
    }
#endif
```

#### Option B: Optimize Count Functions
- Cache device counts and update only on connect/disconnect events
- Return cached value instead of iterating arrays every time

**Implementation Priority:** MEDIUM - Good performance improvement with minimal code changes.

---

## 3. Mouse-to-Joystick Conversion

### Issue: Always-Active Joystick Conversion

**Location:** `src/usb_hid.c` (line 267)

**Problem:**
- `amiga_joystick_port1_set_from_mouse()` is called on **every** mouse event
- This conversion may not always be needed (only when joystick emulation is desired)
- Adds processing overhead to every mouse movement

**Current Code:**
```c
static void handle_event_mouse(...)
{
    // ... mouse button processing ...
    if (report->x || report->y)
        amiga_quad_mouse_set_motion(report->x, report->y);
    
    // Also send mouse input to joystick port 1  ← Always executed
    amiga_joystick_port1_set_from_mouse(report->x, report->y, report->buttons);
}
```

**Impact:** Low-Medium - Processing overhead on every mouse event.

**Recommendations:**

#### Option A: Make Joystick Conversion Optional (Recommended)
- Add a compile-time flag or runtime configuration to enable/disable joystick conversion
- Allow users to disable if joystick emulation is not needed
- Keep default behavior but allow optimization

#### Option B: Optimize Joystick Conversion Logic
- Only process joystick conversion if mouse movement exceeds a threshold
- Skip conversion if no actual mouse movement occurred (only button changes)

**Implementation Priority:** LOW - Useful optimization but less critical than display updates.

---

## 4. Display Update on Plug/Unplug Events

### Issue: Display Updates on Device Events

**Location:** `src/util/debug_cons.c` (lines 82, 102)

**Problem:**
- `dbgcons_print_counters()` is called on every device plug/unplug
- Each call triggers a full display refresh
- Less frequent than keyboard events, but still could be optimized

**Impact:** Low - Device plug/unplug events are infrequent.

**Recommendations:**

#### Option A: Batch Display Updates
- Mark display as dirty and update once per main loop iteration
- Defer display updates until after all processing is complete

#### Option B: Throttle Device Event Display Updates
- Similar to keyboard event throttling, limit updates to once per second

**Implementation Priority:** LOW - Device events are infrequent, optimization impact is minimal.

---

## 5. Additional Optimizations

### A. Main Loop Optimization

**Current:** Tight loop with no delays - CPU runs at 100% even when idle.

**Recommendation:**
- Consider adding a small delay (1-10ms) when no active processing is needed
- Use sleep_us() or sleep_ms() to reduce CPU usage during idle periods
- However, ensure USB and keyboard timing requirements are still met

**Implementation Priority:** LOW - Current approach ensures low-latency input processing.

### B. GPIO Pin State Optimization

**Location:** `src/platform/amiga/joystick_port1.c`

**Issue:** `_aj1_gpio_set()` toggles GPIO direction on every call.

**Recommendation:**
- Only change GPIO direction when switching between input/output modes
- Cache current direction state to avoid unnecessary GPIO configuration calls

**Implementation Priority:** LOW - GPIO operations are fast, optimization impact is minimal.

### C. Memory Allocation in Display Code

**Location:** `src/display/disp_ssd.c`

**Issue:** Display transactions use dynamic memory allocation (malloc/free).

**Recommendation:**
- Consider using a fixed-size transaction buffer pool
- Pre-allocate buffers to avoid malloc/free overhead
- Current implementation already uses a queue, which is good

**Implementation Priority:** LOW - Current implementation is already well-optimized with DMA.

---

## Recommended Implementation Order

1. **HIGH PRIORITY:** Throttle or conditionally disable keyboard display updates (#1)
   - Biggest performance impact
   - Easy to implement
   - No functional changes to input processing

2. **MEDIUM PRIORITY:** Optimize Bluetooth processing (#2)
   - Skip processing when no devices connected
   - Simple early-return checks
   - Good performance improvement

3. **LOW PRIORITY:** Mouse-to-joystick conversion optimization (#3)
   - Make it optional/conditional
   - Useful but less critical

4. **LOW PRIORITY:** Other optimizations (#4, #5)
   - Nice to have but minimal impact

---

## Testing Recommendations

After implementing optimizations:

1. **Measure Keyboard Latency:**
   - Ensure display update throttling doesn't affect keyboard response time
   - Keyboard input processing should remain unaffected

2. **Test Bluetooth Functionality:**
   - Verify Bluetooth devices still connect and function correctly
   - Ensure early-return optimizations don't break device detection

3. **Monitor CPU Usage:**
   - Compare CPU usage before and after optimizations
   - Verify main loop doesn't become too slow with delays

4. **Display Update Frequency:**
   - Verify display still updates appropriately (not too slow)
   - Ensure user feedback is still responsive

---

## Notes

- **Debug vs Release Builds:** Consider making some optimizations conditional based on `DEBUG_MESSAGES` flag
- **Display Updates:** The display is primarily for debugging. If display updates are causing performance issues, consider making them optional in release builds
- **Backward Compatibility:** Ensure optimizations don't break existing functionality
- **Code Readability:** Maintain code clarity while optimizing - avoid premature optimizations that harm maintainability

