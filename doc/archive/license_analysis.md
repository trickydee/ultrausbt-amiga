# License Analysis for Standalone Project

## Current License: Eclipse Public License 2.0 (EPL-2.0)

The original `amigahid-pico` project is licensed under **EPL-2.0**. This analysis identifies which files you've created/modified and what license obligations apply.

## Files Created (New - No EPL-2.0 Header)

These files were created by you and **do NOT have EPL-2.0 headers**:

### Source Code Files:
1. **`src/display/display.c`** - New SSD1306 display implementation
2. **`src/display/display.h`** - Display interface header
3. **`src/bluepad32_init.c`** - Bluepad32 initialization code
4. **`src/bluepad32_init.h`** - Bluepad32 initialization header
5. **`src/bluepad32_platform.c`** - Bluepad32 platform implementation
6. **`src/bluepad32_platform.h`** - Bluepad32 platform header
7. **`src/btstack_config.h`** - BTstack configuration
8. **`src/sdkconfig.h`** - SDK configuration

### Documentation Files (All New):
- `doc/debug_logging_removed.md`
- `doc/device_troubleshooting.md`
- `doc/amiga-code-atari-board-gpio-mappings.md`
- `doc/gpio_allocation_plan.md`
- `doc/gpio_fix_watchdog_summary.md`
- `doc/gpio-fixes-todo.md`
- `doc/oled_display_analysis.md`
- `doc/tusb_init_migration.md`
- `doc/atari_to_amiga_joystick_mapping.md`
- `doc/electrical_operation.md`
- `doc/gpio_pin_mapping.md`
- `doc/gpio_pins.md`
- `doc/joystick_port1_pinout.md`
- `doc/revision5_joystick2_analysis.md`
- `doc/performance_optimizations.md`
- `doc/branch_comparison.md`

### Third-Party Code (Copied from Atari Project):
- `ssd1306/ssd1306.c` - SSD1306 display driver (copied from Atari project)
- `ssd1306/ssd1306.h` - SSD1306 header (copied from Atari project)
- `ssd1306/font.h` - Font definitions (copied from Atari project)

**Note**: The SSD1306 library was copied from your Atari project. You should check the license of that project to determine if it needs to be made available.

## Files Modified (Have EPL-2.0 Header)

These files were modified from the original and **DO have EPL-2.0 headers**:

1. **`src/main.c`** - Modified (GPIO clearing, watchdog, display init, version)
2. **`src/usb_hid.c`** - Modified (Llamatron mode, multi-keyboard, device counting, button mappings)
3. **`src/platform/amiga/joystick_port1.c`** - Modified (mode toggle, memory barriers, button handling)
4. **`src/platform/amiga/joystick_port2.c`** - Modified (initialization, button mappings)
5. **`src/platform/common/gpio_util.c`** - Modified (GPIO clearing, watchdog, pull-up fixes)
6. **`src/config.h`** - Modified (GPIO pin remapping, I2C config, button GPIOs)
7. **`src/platform/amiga/quad_mouse.c`** - Modified (Atari mouse support, Core 1 pause/resume)
8. **`src/util/debug_cons.c`** - Modified (removed old display code)

## License Obligations Summary

### Files That MUST Be Made Available Under EPL-2.0:

**If you distribute your standalone project**, you must make available under EPL-2.0:

1. **All modified files** (files with EPL-2.0 headers that you changed):
   - `src/main.c` (modified portions)
   - `src/usb_hid.c` (modified portions)
   - `src/platform/amiga/joystick_port1.c` (modified portions)
   - `src/platform/amiga/joystick_port2.c` (modified portions)
   - `src/platform/common/gpio_util.c` (modified portions)
   - `src/config.h` (modified portions)
   - `src/platform/amiga/quad_mouse.c` (modified portions)
   - `src/util/debug_cons.c` (modified portions)

2. **New files that modify or extend EPL-2.0 code**:
   - Technically, if your new files are "derivative works" of the EPL-2.0 code, they may need to be EPL-2.0
   - However, if they're independent modules that just link to EPL-2.0 code, they can remain proprietary

### Files That CAN Remain Proprietary:

1. **New files that are independent modules**:
   - `src/display/display.c` and `display.h` (if considered independent)
   - `src/bluepad32_init.c` and `bluepad32_init.h` (if considered independent)
   - `src/bluepad32_platform.c` and `bluepad32_platform.h` (if considered independent)
   - All documentation files (`doc/*.md`)

2. **Configuration files**:
   - `src/btstack_config.h`
   - `src/sdkconfig.h`

## Practical Recommendations

### Option 1: Full EPL-2.0 Compliance (Recommended for Open Source)

If you want to be fully compliant and avoid any ambiguity:

1. **Add EPL-2.0 headers to all new source files** you created
2. **Make all source code available** under EPL-2.0
3. **Keep documentation proprietary** (documentation is typically not covered by EPL-2.0)

**Benefits**:
- Clear compliance with EPL-2.0
- No ambiguity about what needs to be shared
- Can still keep documentation proprietary

### Option 2: Selective Compliance

If you want to keep some code proprietary:

1. **Make available under EPL-2.0**:
   - All modified files (with EPL-2.0 headers)
   - Any new files that directly modify EPL-2.0 functionality

2. **Keep proprietary**:
   - Independent modules (display, bluepad32_init, bluepad32_platform)
   - Documentation
   - Configuration files

**Risks**:
- May need legal review to determine if new files are "derivative works"
- Could be challenged if files are too closely integrated

### Option 3: Dual License

1. **License your new code under a different license** (e.g., MIT, Apache 2.0)
2. **Keep EPL-2.0 for modified original files**
3. **Clearly document which license applies to which files**

## What You Need to Do

### If Distributing Your Standalone Project:

1. **Include original EPL-2.0 license text** (from https://spdx.org/licenses/EPL-2.0)
2. **Preserve all copyright notices** in modified files
3. **Make source code available** for:
   - All modified EPL-2.0 files
   - Any new files you consider derivative works
4. **Document which files are under which license**

### Recommended File Headers:

For new files you want to keep proprietary, add a header like:
```c
/**
 * [Your file description]
 * 
 * Copyright (c) [Year] [Your Name/Company]
 * 
 * This file is proprietary and confidential.
 * All rights reserved.
 */
```

For new files you want to make EPL-2.0, add:
```c
/**
 * [Your file description]
 * 
 * Copyright (c) [Year] [Your Name]
 * 
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which is available at
 * https://www.eclipse.org/legal/epl-2.0/
 */
```

## Questions to Consider

1. **Do you plan to sell products using this code?**
   - EPL-2.0 allows commercial use
   - You must make modifications available, but can keep your own code proprietary

2. **Do you want to keep your improvements proprietary?**
   - You can, but must make EPL-2.0 modifications available
   - New independent modules can remain proprietary

3. **Do you want to contribute back to the original project?**
   - If yes, consider making your code EPL-2.0 compatible
   - This makes it easier to contribute

## Conclusion

For a standalone project, EPL-2.0 is quite permissive. You can:
- ✅ Use the code commercially
- ✅ Keep your new code proprietary (if independent)
- ✅ Sell products using this code
- ⚠️ Must make EPL-2.0 modifications available
- ⚠️ Must preserve copyright notices

**Recommendation**: Add EPL-2.0 headers to your new source files to be safe and compliant, while keeping documentation proprietary. This gives you maximum flexibility while ensuring compliance.

