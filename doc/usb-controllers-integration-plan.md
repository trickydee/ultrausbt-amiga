# USB Controllers Integration Plan

**Date**: 2025-01-XX  
**Branch**: `gpio_pin_mapping_pico_w_fixes-add-usb-devices-from-atari`

## Summary

Adding 6 vendor-specific USB controllers from the Atari IKBD codebase to AmigaHID-Pico:
1. PS3 DualShock 3 (VID: 0x054C, PID: 0x0268) ✅
2. PS4 DualShock 4 (VID: 0x054C, PIDs: 0x05C4, 0x09CC, 0x0BA0) - In Progress
3. Nintendo Switch Pro Controller (VID: 0x057E, PID: 0x2009)
4. Google Stadia Controller (VID: 0x18D1, PID: 0x9400)
5. Xbox Controllers (VID: 0x045E, 7 different PIDs)
6. Nintendo GameCube Adapter (VID: 0x057E, PID: 0x0337)

## Integration Points

### 1. `tuh_hid_mount_cb()` in `src/usb_hid.c`
- Check VID/PID using `tuh_vid_pid_get(dev_addr, &vid, &pid)`
- Call controller-specific mount callbacks
- Example:
```c
uint16_t vid, pid;
tuh_vid_pid_get(dev_addr, &vid, &pid);

if (ps3_is_dualshock3(vid, pid)) {
    ps3_mount_cb(dev_addr);
} else if (ps4_is_dualshock4(vid, pid)) {
    ps4_mount_cb(dev_addr);
}
// ... etc
```

### 2. `tuh_hid_report_received_cb()` or `process_report()` in `src/usb_hid.c`
- Check if device is a vendor-specific controller
- Route to controller-specific report handler
- Example:
```c
uint16_t vid, pid;
tuh_vid_pid_get(dev_addr, &vid, &pid);

if (ps3_is_dualshock3(vid, pid)) {
    ps3_process_report(dev_addr, report, len);
    return;  // Don't process as generic gamepad
} else if (ps4_is_dualshock4(vid, pid)) {
    ps4_process_report(dev_addr, report, len);
    return;
}
// ... etc
// Fall through to generic gamepad handler if no match
```

### 3. Controller Implementation Pattern
Each controller should:
- Map to **Amiga Joystick Port 2** (Port 1 is for mouse/joystick toggle)
- Use `amiga_joystick_port2_set_direction()` and `amiga_joystick_port2_set_button()`
- Handle D-pad priority over analog sticks
- Support deadzone for analog sticks
- Reset port 2 on unmount

### 4. CMakeLists.txt
Add controller source files to build:
```cmake
target_sources(amigahid-pico PRIVATE
    usb_controllers/ps3_controller.c
    usb_controllers/ps4_controller.c
    usb_controllers/switch_controller.c
    usb_controllers/stadia_controller.c
    usb_controllers/xinput_controller.c
    usb_controllers/gamecube_adapter.c
)
```

## Status

- [x] PS3 controller header and implementation
- [ ] PS4 controller header and implementation
- [ ] Switch controller header and implementation
- [ ] Stadia controller header and implementation
- [ ] Xbox controller header and implementation
- [ ] GameCube adapter header and implementation
- [ ] Integration into usb_hid.c
- [ ] CMakeLists.txt update
- [ ] Build and test


