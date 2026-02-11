# TODO - Outstanding Work

This document tracks outstanding tasks and improvements for the amigahid-pico project.

## License Compliance

### High Priority
- [ ] **Add EPL-2.0 license headers to new source files**
  - [ ] `src/display/display.c` - Add EPL-2.0 header
  - [ ] `src/display/display.h` - Add EPL-2.0 header
  - [ ] `src/bluepad32_init.c` - Add EPL-2.0 header
  - [ ] `src/bluepad32_init.h` - Add EPL-2.0 header
  - [ ] `src/bluepad32_platform.c` - Add EPL-2.0 header
  - [ ] `src/bluepad32_platform.h` - Add EPL-2.0 header
  - [ ] `src/btstack_config.h` - Add EPL-2.0 header (or proprietary header)
  - [ ] `src/sdkconfig.h` - Add EPL-2.0 header (or proprietary header)
  
- [ ] **Determine license for SSD1306 library** (copied from Atari project)
  - [ ] Check Atari project license for `ssd1306/` files
  - [ ] Add appropriate license headers to SSD1306 files
  
- [ ] **Create LICENSE file** in project root
  - [ ] Include EPL-2.0 license text
  - [ ] Document which files are under which license
  - [ ] List third-party licenses (Bluepad32, TinyUSB, Pico SDK, etc.)

- [ ] **Update README.md with license information**
  - [ ] Document license obligations
  - [ ] Explain what code must be made available
  - [ ] Link to license analysis document

**Reference**: See `doc/license_analysis.md` for detailed analysis

## Atari Mouse Support Integration

### High Priority
- [ ] **Merge Atari mouse support from `atari-mouse-support` branch**
  - [ ] **Add mouse type enum to `quad_mouse.h`**:
    ```c
    typedef enum {
        MOUSE_TYPE_AMIGA = 0,
        MOUSE_TYPE_ATARI = 1
    } mouse_type_t;
    ```
  
  - [ ] **Add mouse type functions to `quad_mouse.h`**:
    - [ ] `void amiga_quad_mouse_set_type(mouse_type_t type);`
    - [ ] `mouse_type_t amiga_quad_mouse_get_type(void);`
    - [ ] `void amiga_quad_mouse_toggle_type(void);`
  
  - [ ] **Implement GPIO pin swapping in `quad_mouse.c`**:
    - [ ] Add `volatile mouse_type_t g_mouse_type = MOUSE_TYPE_AMIGA;` global variable
    - [ ] Add `get_gpio_v()` function: Returns `QM1_AMIGA_HQ` for Atari, `QM1_AMIGA_V` for Amiga
    - [ ] Add `get_gpio_hq()` function: Returns `QM1_AMIGA_V` for Atari, `QM1_AMIGA_HQ` for Amiga
    - [ ] Update `amiga_quad_mouse_motion()` to use `get_gpio_v()` and `get_gpio_hq()` instead of direct GPIO constants
    - [ ] Add memory barriers (`__sync_synchronize()`) around mouse type reads for cross-core safety
    - [ ] **GPIO Swap Details**:
      - **Amiga Mode (default)**: 
        - V (vertical) → DB-9 Pin 1 (GPIO 10)
        - HQ (horizontal quadrature) → DB-9 Pin 4 (GPIO 13)
      - **Atari Mode**:
        - V and HQ are swapped: V → DB-9 Pin 4, HQ → DB-9 Pin 1
        - This swaps the X and Y axes to match Atari ST/TT mouse pinout
  
  - [ ] **Add mouse type toggle to display**:
    - [ ] In `display_show_devices()`, show mouse type when Port 1 is in mouse mode:
      ```c
      if (!is_joy_mode) {
          mouse_type_t mouse_type = amiga_quad_mouse_get_type();
          sprintf(buf, "Mouse: %s", mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
          ssd1306_draw_string(&disp, 0, 36, 1, buf);
      }
      ```
    - [ ] Add RIGHT button handler on DEVICES screen to toggle mouse type:
      ```c
      if (current_screen == DISPLAY_SCREEN_DEVICES) {
          amiga_quad_mouse_toggle_type();
          display_show_devices();  // Refresh to show new type
      }
      ```
  
  - [ ] **Remove debug code from atari-mouse-support branch**:
    - [ ] Remove `printf("[MOUSE_BTN]...")` debug logging from `amiga_quad_mouse_button()`
    - [ ] Remove any other debug code added for hardware troubleshooting (user confirmed issues were hardware-related)

**Reference**: Changes are in `atari-mouse-support` branch. The GPIO swap swaps DB-9 pins 1 and 4 (V and HQ signals) to support Atari ST/TT mice which have different pinout than Amiga mice.

## Code Quality & Maintenance

### Medium Priority
- [ ] **Remove unused variables** (compiler warnings)
  - [ ] `src/display/display.c` - Remove unused `mode_line`, `mode_buf` variables
  - [ ] `src/display/display.c` - Remove unused `name`, `buf` variables in `display_show_bt_names()`

- [ ] **Code cleanup**
  - [ ] Review and remove any remaining debug code
  - [ ] Standardize code formatting
  - [ ] Add missing function documentation comments

## Testing & Validation

### Medium Priority
- [ ] **Hardware testing**
  - [ ] Test GPIO 26 (fire button) on second Pico W board
  - [ ] Verify all GPIO remappings work correctly (GPIOs 2, 3, 27, 28)
  - [ ] Test Atari mouse mode on actual Atari hardware
  - [ ] Verify all joystick buttons work correctly on both ports

- [ ] **Bluetooth testing**
  - [ ] Test mode display shows "USB+BT" correctly when Bluetooth is enabled
  - [ ] Verify RST button label appears correctly
  - [ ] Test Bluetooth pairing persistence across power cycles
  - [ ] Verify multi-keyboard support works correctly

- [ ] **Display testing**
  - [ ] Verify splash screen shows "ultramegausb.com" correctly
  - [ ] Test all three display screens (SPLASH, DEVICES, BT_NAMES)
  - [ ] Verify button navigation works correctly
  - [ ] Test device counter updates in real-time

## Documentation

### Low Priority
- [ ] **Update main README.md**
  - [ ] Add information about Atari board compatibility
  - [ ] Document GPIO pin mappings for Revision 5
  - [ ] Add build instructions for all board types
  - [ ] Document new features (Llamatron mode, multi-keyboard, etc.)

- [ ] **Create user guide**
  - [ ] Document how to use Llamatron twinstick mode
  - [ ] Explain how to toggle Port 1 between mouse/joystick
  - [ ] Document Atari mouse mode toggle
  - [ ] Explain Bluetooth pairing and management

- [ ] **Hardware documentation**
  - [ ] Document GPIO pin conflicts that were resolved
  - [x] Create hardware remapping guide for Button 2/3
  - [x] Port 2 Button 2: Currently using GPIO 27 (ADC1). Was temporarily moved to GPIO 7 during debugging but reverted back to GPIO 27.
  - [ ] Document electrical protection requirements (5V back-feeding)

## Future Enhancements

### Low Priority
- [ ] **Hardware protection**
  - [ ] Research level shifters for GPIO protection
  - [ ] Document recommended protection circuits
  - [ ] Add warnings about 5V back-feeding risks

- [ ] **Performance optimizations**
  - [ ] Review GPIO update frequency
  - [ ] Optimize display refresh rate
  - [ ] Reduce memory usage if possible

- [ ] **Feature requests**
  - [ ] Consider additional gamepad button mappings
  - [ ] Support for more Bluetooth device types
  - [ ] Additional display screens/information

## Known Issues

### Resolved (Keep for Reference)
- ✅ GPIO 26 fire button issue on first Pico W (hardware damage from 5V back-feeding)
- ✅ Mode display showing "USB" instead of "USB+BT" (fixed in v1.0.11)
- ✅ Circle button triggering UP movement (fixed with D-pad filter)
- ✅ Square button causing Bluetooth disconnect (fixed with GPIO remapping)
- ✅ Joystick Port 1 LEFT/RIGHT not working (fixed with memory barriers)
- ✅ Stadia controller hangs (fixed with Core 1 pause/resume)
- ✅ DS5 pairing on Pico 2 W (fixed with flash-safe execution)

### Open Issues
- [ ] None currently known

## Version History Tracking

- **v1.0.11** - Fixed mode display, removed debug logging, updated branding
- **v1.0.10** - Removed debug logging, updated splash screen
- **v1.0.9** - Enhanced GPIO initialization safety
- **v1.0.8** - GPIO pull-up fixes, extensive debug logging
- **v1.0.7** - GPIO button remapping (GPIOs 2, 3, 27, 28)
- **v1.0.6** - Atari mouse support
- **v1.0.5** - GPIO conflict fixes
- **v1.0.4** - Bluetooth device names display
- **v1.0.3** - Joystick Port 1 fixes
- **v1.0.2** - Stadia controller fixes
- **v1.0.1** - Multi-keyboard support, Bluetooth persistence
- **v1.0.0** - Initial release with Atari board compatibility

---

**Last Updated**: 2024 (after v1.0.11)
**Next Review**: After license compliance work completed

