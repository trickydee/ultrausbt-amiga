# Additional USB Controllers Available in Atari IKBD Codebase

**Date**: 2025-01-XX  
**Branch**: `gpio_pin_mapping_pico_w_fixes-add-usb-devices-from-atari`

## Summary

The Atari IKBD codebase has **6 vendor-specific USB controller implementations** that are not currently in AmigaHID-Pico. These controllers use direct GPIO control (same architecture as AmigaHID-Pico), making them easy to port.

## Controllers Available in Atari IKBD

### 1. **PS3 DualShock 3** (`ps3_controller.c`)
- **VID**: 0x054C (Sony)
- **PID**: 0x0268 (DualShock 3)
- **Features**:
  - Full button support (D-pad, face buttons, shoulders, triggers)
  - Analog sticks with deadzone
  - Pressure-sensitive buttons (if supported)
  - Motion sensors (gyro/accel)
- **Status**: ✅ Complete implementation
- **Files**: `src/ps3_controller.c`, `include/ps3_controller.h`

### 2. **PS4 DualShock 4** (`ps4_controller.c`)
- **VID**: 0x054C (Sony)
- **PIDs**: 
  - 0x05C4 (DS4 v1)
  - 0x09CC (DS4 v2)
  - 0x0BA0 (PS4 Wireless Adapter PC)
- **Features**:
  - Full button support
  - Analog sticks with deadzone (50 units)
  - Touchpad button
  - Motion sensors (gyro/accel)
  - Light bar control
- **Status**: ✅ Complete implementation
- **Files**: `src/ps4_controller.c`, `include/ps4_controller.h`

### 3. **Nintendo Switch Pro Controller** (`switch_controller.c`)
- **VID**: 0x057E (Nintendo)
- **PIDs**:
  - 0x2009 (Switch Pro Controller)
  - 0x200E (Joy-Con Charge Grip)
  - 0x2017 (SNES Controller NSO)
- **Third-party support**:
  - PowerA Fusion Wireless Arcade Stick
  - PowerA Wired Plus
  - PowerA Wireless
- **Features**:
  - Full button support (including capture/home buttons)
  - Analog sticks with deadzone (20 units)
  - Delayed initialization sequence (1 second delay)
  - Motion sensors
- **Status**: ✅ Complete implementation with initialization sequence
- **Files**: `src/switch_controller.c`, `include/switch_controller.h`

### 4. **Google Stadia Controller** (`stadia_controller.c`)
- **VID**: 0x18D1 (Google)
- **PID**: 0x9400
- **Features**:
  - Full button support
  - Analog sticks with deadzone (20 units)
  - D-pad hat switch support
  - Llamatron dual-stick mode support
- **Status**: ✅ Complete implementation
- **Files**: `src/stadia_controller.c`, `include/stadia_controller.h`

### 5. **Xbox Controllers** (`xinput.c`, `xinput_host.c`)
- **VID**: 0x045E (Microsoft)
- **PIDs**:
  - Multiple Xbox One controller variants (7 different PIDs)
  - Xbox 360 (wired/wireless)
  - Xbox Series X|S
- **Features**:
  - XInput protocol support
  - Full button support
  - Analog sticks with deadzone (8000 units, ~25%)
  - Triggers
  - Initialization packet required
- **Status**: ✅ Complete implementation with XInput protocol
- **Files**: `src/xinput.c`, `src/xinput_host.c`, `include/xinput.h`, `include/xinput_host.h`

### 6. **Nintendo GameCube Adapter** (`gamecube_adapter.c`)
- **VID**: 0x057E (Nintendo)
- **PID**: 0x0337 (GameCube Adapter for WiiU/Switch)
- **Features**:
  - 4-port adapter support
  - Native GameCube controller protocol
  - Full button support (including analog triggers)
  - Analog stick support
  - Rumble support
- **Status**: ✅ Complete implementation
- **Files**: `src/gamecube_adapter.c`, `src/gamecube_vendor.c`, `include/gamecube_adapter.h`

## Current AmigaHID-Pico Support

### USB HID (Current)
- ✅ **Generic HID gamepads** - Basic parsing via `handle_event_gamepad()`
- ✅ **USB keyboards** - Full support
- ✅ **USB mice** - Full support
- ⚠️ **No vendor-specific controllers** - Relies on generic HID parsing

### Bluetooth (Current)
- ✅ **Xbox controllers** (via Bluepad32)
- ✅ **PlayStation controllers** (DS3/DS4/DualSense via Bluepad32)
- ✅ **Switch Pro Controller** (via Bluepad32)
- ✅ **Stadia controller** (via Bluepad32)
- ✅ **Generic HID gamepads** (fallback)

## What's Missing in AmigaHID-Pico

### USB-Specific Controllers (Not Available via Bluetooth)
1. **PS3 DualShock 3** - USB only (no Bluetooth support in DS3)
2. **PS4 DualShock 4** - USB support (Bluetooth also available via Bluepad32)
3. **Switch Pro Controller** - USB support (Bluetooth also available via Bluepad32)
4. **Stadia Controller** - USB support (Bluetooth also available via Bluepad32)
5. **Xbox Controllers** - USB XInput support (Bluetooth also available via Bluepad32)
6. **GameCube Adapter** - USB only (4-port adapter)

## Porting Effort Estimate

### Easy to Port (1-2 days each)
- **PS3 DualShock 3** - Simple report format, direct GPIO mapping
- **PS4 DualShock 4** - Similar to PS3, well-documented format
- **Stadia Controller** - Standard HID format, straightforward
- **Switch Pro Controller** - Requires initialization sequence, but well-documented

### Medium Effort (2-3 days each)
- **Xbox Controllers** - XInput protocol, requires initialization packet, endpoint discovery
- **GameCube Adapter** - 4-port support, native protocol, more complex

## Recommended Porting Order

1. **PS4 DualShock 4** (High value, common controller)
2. **PS3 DualShock 3** (Easy, legacy support)
3. **Switch Pro Controller** (Popular, requires init sequence)
4. **Stadia Controller** (Easy, good for testing)
5. **Xbox Controllers** (Medium effort, but high value)
6. **GameCube Adapter** (Lower priority, niche use case)

## Integration Pattern

All Atari controllers follow the same pattern:

1. **VID/PID Detection**:
   ```c
   bool ps4_is_dualshock4(uint16_t vid, uint16_t pid) {
       if (vid != PS4_VENDOR_ID) return false;
       switch (pid) {
           case PS4_DS4_PID_V1:
           case PS4_DS4_PID_V2:
               return true;
       }
       return false;
   }
   ```

2. **Report Processing**:
   ```c
   bool ps4_process_report(uint8_t dev_addr, const uint8_t* report, uint16_t len) {
       // Parse report
       // Map to Atari joystick ports (replace with Amiga GPIO)
   }
   ```

3. **Integration in `hid_app_host.c`**:
   ```c
   void tuh_hid_mount_cb(...) {
       uint16_t vid, pid;
       tuh_vid_pid_get(dev_addr, &vid, &pid);
       
       if (ps4_is_dualshock4(vid, pid)) {
           // Allocate controller
       }
   }
   
   void tuh_hid_report_received_cb(...) {
       if (ps4_is_dualshock4(vid, pid)) {
           ps4_process_report(dev_addr, report, len);
       }
   }
   ```

## Key Differences from AmigaHID-Pico

### Atari IKBD
- Maps to **Atari joystick ports** (GPIO inputs with pull-ups)
- Uses **Atari-specific functions** (`atari_joystick_set_direction()`)
- Supports **Llamatron mode** (dual-stick)

### AmigaHID-Pico
- Maps to **Amiga joystick ports** (GPIO outputs, active low)
- Uses **Amiga-specific functions** (`amiga_joystick_port2_set_direction()`)
- Supports **Llamatron mode** (already implemented)

## Porting Steps (Per Controller)

1. **Copy controller files** from Atari IKBD
2. **Replace Atari GPIO calls** with Amiga GPIO calls:
   - `atari_joystick_set_direction()` → `amiga_joystick_port2_set_direction()`
   - `atari_joystick_set_button()` → `amiga_joystick_port2_set_button()`
3. **Update VID/PID definitions** (if needed)
4. **Integrate into `usb_hid.c`**:
   - Add VID/PID check in `tuh_hid_mount_cb()`
   - Add report processing in `tuh_hid_report_received_cb()`
5. **Test and adjust deadzones** (if needed)

## Estimated Total Effort

- **All 6 controllers**: 10-15 days
- **High-value controllers only** (PS3, PS4, Switch, Stadia): 6-8 days
- **Quick wins** (PS3, PS4, Stadia): 3-4 days


