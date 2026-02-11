# Joypad OS Investigation - Useful Features for AmigaHID-Pico

**Date**: 2025-01-XX  
**Branch**: `joystickos-investigation`  
**Project**: [joypad-os](https://github.com/joypad-ai/joypad-os) (formerly USBRetro)

## Executive Summary

Joypad OS is a comprehensive, modular firmware platform for building controller adapters on RP2040. It provides a well-architected foundation with excellent USB HID parsing, Bluetooth support, input routing, and device abstraction. While it's designed for different use cases (retro console adapters), several components could be valuable for AmigaHID-Pico.

## Key Features & Potential Value

### ✅ **Highly Useful Components**

#### 1. **Unified Input Event System** (`core/input_event.h`)
- **What it is**: Standardized `input_event_t` structure that normalizes all input types (gamepads, mice, keyboards) into a single format
- **Value for AmigaHID-Pico**:
  - Clean abstraction for handling USB HID, Bluetooth, and native inputs
  - Normalized analog stick values (0-255, centered at 128)
  - Button bitmap using W3C Gamepad API order
  - Support for motion data (gyro/accel), pressure-sensitive buttons, chatpad
  - Device type classification (gamepad, mouse, keyboard, etc.)
- **Current AmigaHID-Pico state**: Uses ad-hoc structures and direct GPIO manipulation
- **Effort to integrate**: Medium (would require refactoring current input handling)

#### 2. **Input Interface Abstraction** (`core/input_interface.h`)
- **What it is**: Plugin-style interface for different input sources (USB host, Bluetooth, native, GPIO)
- **Value for AmigaHID-Pico**:
  - Clean separation between input sources and output targets
  - Easy to add new input types (e.g., WiFi controllers, UART)
  - Consistent API for polling/status checking
- **Current AmigaHID-Pico state**: Direct calls to `usb_hid.c` and `bluepad32_platform.c`
- **Effort to integrate**: Medium-High (architectural change)

#### 3. **Router System** (`core/router/router.h`)
- **What it is**: Zero-latency N:M input→output routing with multiple modes:
  - **SIMPLE**: 1:1 fixed routing (current AmigaHID-Pico behavior)
  - **MERGE**: N:1 merge inputs (could enable multiple gamepads → single port)
  - **BROADCAST**: 1:N broadcast (one input → multiple outputs)
  - **CONFIGURABLE**: N:M user-defined routing tables
- **Value for AmigaHID-Pico**:
  - Could enable advanced features like:
    - Multiple gamepads merged to single port
    - One gamepad controlling both ports simultaneously
    - Input transformations (mouse→analog stick, spinner accumulation)
  - Lock-free, zero-copy design for low latency
- **Current AmigaHID-Pico state**: Direct mapping (gamepad 0 → port 2, gamepad 1 → port 1)
- **Effort to integrate**: High (significant architectural change, but powerful)

#### 4. **USB HID Device Registry** (`bt/bthid/bthid_registry.c`)
- **What it is**: Device driver registry that matches controllers by VID/PID, name, or Class of Device
- **Value for AmigaHID-Pico**:
  - Better device-specific handling (e.g., DualSense, Stadia, Xbox)
  - Extensible driver system for new controllers
  - Consistent device identification across USB and Bluetooth
- **Current AmigaHID-Pico state**: Basic VID/PID checks in `usb_hid.c` and `bluepad32_platform.c`
- **Effort to integrate**: Medium (could improve device compatibility)

#### 5. **Button Layout Transformations** (`core/input_event.h`)
- **What it is**: Functions to transform button mappings between different physical layouts (6-button SEGA, PCEngine, etc.)
- **Value for AmigaHID-Pico**:
  - Could help with CD32 pad support (if implemented)
  - Useful for handling different controller button arrangements
- **Current AmigaHID-Pico state**: Direct button mapping
- **Effort to integrate**: Low-Medium (useful if adding CD32 or other complex controllers)

### ⚠️ **Potentially Useful (Context-Dependent)**

#### 6. **Pad Input System** (`pad/pad_input.c`)
- **What it is**: GPIO-based input system for custom controllers (buttons/sticks wired directly to GPIO)
- **Value for AmigaHID-Pico**:
  - If you want to support direct GPIO controllers (arcade sticks, custom pads)
  - I2C expander support for more buttons
  - ADC support for analog sticks
- **Current AmigaHID-Pico state**: No direct GPIO controller support
- **Effort to integrate**: Low (standalone system, but limited use case)

#### 7. **Profile System** (`core/services/profiles/`)
- **What it is**: Button remapping profiles stored in flash
- **Value for AmigaHID-Pico**:
  - Could enable user-configurable button mappings
  - Per-controller profiles
- **Current AmigaHID-Pico state**: Fixed button mappings
- **Effort to integrate**: Medium (requires UI for profile selection)

#### 8. **Hotkey System** (`core/services/hotkeys/`)
- **What it is**: Button combo detection for special functions
- **Value for AmigaHID-Pico**:
  - Could enable button combos for mode switching (e.g., L1+R1+Start = toggle mouse type)
  - Alternative to OLED button navigation
- **Current AmigaHID-Pico state**: OLED-based UI only
- **Effort to integrate**: Low-Medium (useful feature)

### ❌ **Not Directly Applicable**

- **Console Output Protocols**: GameCube, Dreamcast, 3DO, PCEngine, etc. (different target than Amiga)
- **Native Input Protocols**: SNES, N64, GameCube joybus (not needed for Amiga)
- **USB Device Output Modes**: XInput, PS3/PS4/Switch modes (Amiga uses GPIO, not USB device)

## Architecture Comparison

### Joypad OS Architecture
```
Input Sources → Input Interfaces → Router → Output Interfaces → Console Protocols
     ↓              ↓                ↓            ↓                  ↓
  USB Host      USB Interface    Routing      GameCube         GPIO Output
  Bluetooth     BT Interface     Logic        Dreamcast        (PIO timing)
  Native        Native Interface              PCEngine
  GPIO          Pad Interface                  USB Device
```

### Current AmigaHID-Pico Architecture
```
USB HID → usb_hid.c → Direct GPIO Control → Amiga Ports
Bluetooth → bluepad32 → usb_hid.c → Direct GPIO Control → Amiga Ports
Mouse → quad_mouse.c (Core 1) → Direct GPIO Control → Amiga Port 1
```

## Recommended Integration Strategy

### Phase 1: Low-Risk Improvements (High Value)
1. **Adopt `input_event_t` structure**:
   - Replace ad-hoc gamepad structures with standardized format
   - Normalize analog values to 0-255 range
   - Benefits: Cleaner code, easier to add new input types

2. **Improve USB HID parsing**:
   - Study joypad-os USB HID device drivers
   - Improve device-specific handling (DualSense, Stadia, etc.)
   - Better report parsing for edge cases

3. **Button layout transformations**:
   - Useful if implementing CD32 pad support
   - Helps with different controller button arrangements

### Phase 2: Medium-Risk Architectural Changes (High Value)
1. **Input Interface abstraction**:
   - Create `InputInterface` for USB, Bluetooth, GPIO
   - Cleaner separation of concerns
   - Easier to add new input types (WiFi, UART)

2. **Router system** (if advanced features needed):
   - Enable multiple gamepads → single port
   - Input merging/broadcasting
   - Input transformations (mouse→analog)

### Phase 3: Advanced Features (Lower Priority)
1. **Profile system**: User-configurable button mappings
2. **Hotkey system**: Button combos for mode switching
3. **Pad input system**: Direct GPIO controller support

## Code Quality Observations

### Strengths
- **Well-documented**: Excellent comments, architecture docs (CLAUDE.md)
- **Modular design**: Clean separation of concerns
- **Extensible**: Easy to add new devices/protocols
- **Performance-focused**: Lock-free routing, zero-copy design
- **Standards-compliant**: W3C Gamepad API button order

### Considerations
- **Complexity**: More complex than current AmigaHID-Pico (may be overkill)
- **License**: Apache-2.0 (compatible with EPL-2.0, but need to check dependencies)
- **Dependencies**: Uses TinyUSB, BTstack, Pico SDK (same as AmigaHID-Pico)
- **Size**: Larger codebase (may not fit if flash is constrained)

## Specific Code References

### USB HID Parsing
- **Location**: `src/usb/usbh/` (USB host drivers)
- **Key files**: Device-specific drivers in `src/bt/bthid/devices/`
- **Pattern**: VID/PID matching → device-specific driver → normalized `input_event_t`

### Bluetooth HID
- **Location**: `src/bt/bthid/`
- **Key files**: `bthid.c`, `bthid_registry.c`
- **Pattern**: Similar to USB - device registry → driver → `input_event_t`

### Input Event Structure
- **Location**: `src/core/input_event.h`
- **Key features**:
  - Normalized analog values (0-255, centered at 128)
  - Button bitmap (W3C Gamepad API order)
  - Device type classification
  - Motion data support
  - Pressure-sensitive buttons

### Router System
- **Location**: `src/core/router/router.c`
- **Key features**:
  - Lock-free, zero-copy design
  - Multiple routing modes (SIMPLE, MERGE, BROADCAST, CONFIGURABLE)
  - Input transformations (mouse→analog, spinner accumulation)
  - Push-based (tap callbacks) and pull-based (polling) APIs

## Conclusion

Joypad OS offers several valuable components that could improve AmigaHID-Pico:

1. **Immediate value**: Adopt `input_event_t` structure and improve USB HID parsing
2. **Medium-term**: Consider input interface abstraction and router system (if advanced features needed)
3. **Long-term**: Profile system, hotkey system (if user-configurable features desired)

**Recommendation**: Start with Phase 1 improvements (low-risk, high-value). The `input_event_t` structure and improved USB HID parsing would provide immediate benefits without major architectural changes.

**License Note**: Joypad OS is Apache-2.0 licensed, which is compatible with EPL-2.0. However, check all dependencies (TinyUSB, BTstack, etc.) for license compatibility.


