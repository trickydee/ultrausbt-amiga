# Joypad OS Device Support Comparison

**Date**: 2025-01-XX  
**Branch**: `joystickos-investigation`

## Summary

If we adopt Joypad OS's USB HID parsing system, we would gain **vendor-specific drivers for ~20+ unique controller models** that handle quirks, special report formats, and button mappings. However, most modern controllers already work with AmigaHID-Pico's generic HID parser - the benefit is **better compatibility and more reliable operation** rather than adding entirely new devices.

## Current AmigaHID-Pico Support

### USB HID (via TinyUSB)
- ✅ **Generic HID gamepads** - Works with most standard USB HID joysticks
- ✅ **USB keyboards** - Full support
- ✅ **USB mice** - Full support
- ⚠️ **Xbox controllers** - Works via XInput protocol (if supported by TinyUSB)
- ⚠️ **PlayStation controllers** - May work but no special handling
- ⚠️ **Switch controllers** - May work but no special handling

### Bluetooth (via Bluepad32)
- ✅ **Xbox controllers** (One/Series Bluetooth models)
- ✅ **PlayStation controllers** (DS3/DS4/DualSense)
- ✅ **Switch Pro Controller**
- ✅ **Stadia controller** (with special handling)
- ✅ **Generic HID gamepads** (fallback)

**Note**: Bluepad32 already has good device support, so Bluetooth is less of a concern.

## Joypad OS USB HID Device Drivers

### Sony Controllers (4 drivers)
1. **DualShock 3 (PS3)**
   - VID: 0x054C, PID: 0x0268
   - Special features: Pressure-sensitive buttons, motion sensors
   - Also handles: Hori Fighting Stick 3, Mad Catz fight sticks, Qanba sticks, Logitech F310 (PS3 mode)

2. **DualShock 4 (PS4)**
   - VID: 0x054C, PIDs: 0x09CC, 0x05C4, 0x0BA0
   - Special features: Touchpad button, light bar, motion sensors
   - Also handles: Hori Fighting Commander 4, Hori RAP V, Razer Panthera, Brook fight boards, Mad Catz fight sticks, Qanba sticks, PowerA FUSION, etc. (20+ arcade sticks)

3. **DualSense (PS5)**
   - VID: 0x054C, PID: 0x0CE6
   - Special features: Adaptive triggers, haptic feedback, touchpad

4. **PlayStation Classic Controller**
   - VID: 0x054C, PID: 0x0CDA
   - Special features: Simplified button layout

### Nintendo Controllers (3 drivers)
5. **Switch Pro Controller**
   - VID: 0x057E, PIDs: 0x2009, 0x200E, 0x2017
   - Special features: Motion sensors, capture button, home button
   - Also handles: Joy-Con Charge Grip, SNES Controller (NSO)

6. **Switch 2 Pro Controller**
   - VID: 0x057E, PIDs: 0x2069, 0x2073
   - Special features: Newer Switch 2 controllers, GameCube NSO controller

7. **GameCube Adapter**
   - VID: 0x057E, PID: 0x0337
   - Special features: 4-port adapter, native GameCube controllers

### Microsoft Controllers (1 driver)
8. **Sidewinder DualStrike**
   - VID: 0x045E, PID: 0x0028
   - Special features: Unique dual-stick layout

**Note**: Xbox controllers (360/One/Series) use XInput protocol, not USB HID, so they're handled separately.

### Google Controllers (1 driver)
9. **Stadia Controller**
   - VID: 0x18D1, PID: 0x9400
   - Special features: Already supported in AmigaHID-Pico via Bluepad32

### 8BitDo Controllers (3 drivers)
10. **8BitDo BTA (Wireless USB Adapter)**
    - Handles: Grey/Red adapters, Black/Red adapters
    - Special features: Mode switching, turbo functionality

11. **8BitDo M30**
    - Special features: 6-button layout, analog triggers

12. **8BitDo PCE**
    - Special features: PCEngine 2.4g controller support

### Hori Controllers (2 drivers)
13. **Hori Horipad**
    - VID: 0x0F0D, PID: 0x00C1
    - Special features: Switch-compatible gamepad

14. **Hori Pokken Tournament Controller**
    - VID: 0x0F0D, PID: 0x0092
    - Special features: Wii U fight stick

### Other Controllers (4 drivers)
15. **Logitech Wingman Action Pad**
    - VID: 0x046D, PID: 0xC20B
    - Special features: Classic PC gamepad

16. **Sega Astrocity Mini Controller**
    - VID: 0x0CA3, PIDs: 0x0028, 0x0027, 0x0024
    - Special features: Arcade stick, 6-button layout
    - Also handles: 8BitDo M30 (2.4g variant)

17. **Raphnet PCE Adapter**
    - VID: 0x289B, PID: 0x0050
    - Special features: PC Engine adapter

18. **Triple Adapter v1/v2**
    - VID: 0x2341, PID: 0x8036 (Arduino Leonardo)
    - Special features: NES/SNES/Genesis adapter

### Generic Drivers (3)
19. **Generic HID Gamepad** - Fallback for standard USB HID joysticks
20. **Generic HID Keyboard** - Standard USB keyboards
21. **Generic HID Mouse** - Standard USB mice

## Key Differences

### What Joypad OS Adds

1. **Vendor-Specific Quirks Handling**:
   - Special report formats (e.g., DualShock 3 pressure-sensitive buttons)
   - Button mapping corrections (e.g., Switch Pro capture/home buttons)
   - Analog stick calibration (e.g., different deadzones per controller)
   - Motion sensor support (gyro/accel for DS3/DS4/DualSense)

2. **Arcade Stick Support**:
   - 20+ arcade stick models (Hori, Mad Catz, Qanba, Razer, Brook, etc.)
   - Many use DS3/DS4 VID/PID but need special handling

3. **Adapter Support**:
   - GameCube Adapter (4 ports)
   - 8BitDo wireless adapters
   - Raphnet PCE adapter
   - Triple adapter (NES/SNES/Genesis)

4. **Legacy Controller Support**:
   - Logitech Wingman
   - Microsoft Sidewinder DualStrike
   - PlayStation Classic controller

### What AmigaHID-Pico Already Has

- ✅ Generic HID parsing (works for most modern controllers)
- ✅ Bluepad32 Bluetooth support (excellent device compatibility)
- ✅ Stadia controller support (via Bluepad32)
- ✅ Basic Xbox/PlayStation/Switch support (via generic HID or Bluepad32)

## Estimated Additional Device Support

### Directly New Devices (~10-15)
- GameCube Adapter (4 ports)
- 8BitDo wireless adapters (BTA)
- Raphnet PCE adapter
- Triple adapter (NES/SNES/Genesis)
- Microsoft Sidewinder DualStrike
- Logitech Wingman Action Pad
- PlayStation Classic controller
- Switch 2 Pro Controller (newer models)
- Various arcade sticks (if they don't work with generic HID)

### Improved Compatibility (~20-30)
- DualShock 3 (pressure-sensitive buttons, motion)
- DualShock 4 (touchpad, motion, better button mapping)
- DualSense (adaptive triggers, better button mapping)
- Switch Pro (motion, capture/home buttons)
- Various arcade sticks (better button mapping, quirks)

## Recommendation

**The real value isn't in adding new devices, but in improving compatibility and reliability for existing devices.**

### High Value Improvements:
1. **DualShock 3/4/DualSense** - Better button mapping, motion support, touchpad
2. **Switch Pro** - Capture/home button support, motion sensors
3. **Arcade Sticks** - Many use DS3/DS4 VID/PID but need special handling
4. **GameCube Adapter** - 4-port support (if needed)

### Medium Value Improvements:
5. **8BitDo adapters** - Mode switching, turbo functionality
6. **Legacy controllers** - Wingman, Sidewinder (if users have them)

### Low Value (Already Work):
- Generic HID gamepads (already work)
- Xbox controllers (work via XInput/Bluepad32)
- Stadia (already supported)

## Conclusion

**Estimated additional devices: ~10-15 new devices, ~20-30 improved compatibility**

However, the **real benefit** is:
- **Better reliability** for controllers that may have quirks
- **More features** (motion sensors, touchpad, pressure-sensitive buttons)
- **Better button mapping** for arcade sticks and specialized controllers
- **Adapter support** (GameCube, 8BitDo, Raphnet)

**Recommendation**: If adopting Joypad OS's USB HID parsing, focus on:
1. High-value controllers (DS3/DS4/DualSense, Switch Pro)
2. Arcade stick support (many users have these)
3. Adapter support (if needed for your use case)

The generic HID parser already handles most modern controllers, so the vendor-specific drivers are more about **polish and reliability** than adding entirely new device support.


