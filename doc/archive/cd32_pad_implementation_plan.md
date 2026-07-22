# CD32 Pad Protocol Implementation Plan

> **Superseded for implementation:** Use **[`CD32_BUILD_SPEC.md`](./CD32_BUILD_SPEC.md)** (same archive folder; historical build spec).  
> This file is kept for protocol background. The GPIO mapping below (Clock/Latch on GPIO 2/3) was **incorrect** for Rev 5 — CD32 Clock/Latch/Data are on **direction pins** (DB-9 pins 2–4 → GPIO 11–13 Port 1, GPIO 20–22 Port 2).

## Overview

This document outlines the plan for implementing CD32 pad protocol support on joystick port 1 as an alternative to the standard Amiga joystick protocol. The CD32 protocol uses serial communication (clock/data/latch) instead of parallel GPIO, which could solve voltage threshold issues with buttons 2 and 3.

## References

- **KTRL_CD32 Implementation**: https://github.com/MickGyver/KTRL-CD32/blob/master/Source/KTRL_CD32_Rev2/KTRL_CD32_Rev2.ino
- **9pin2supergun Implementation**: https://github.com/turmoni/9pin2supergun/blob/main/src/main.rs
- **Amiga Hardware Manual**: https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node017E.html

## CD32 Protocol Overview

### Protocol Characteristics

The CD32 pad protocol is a **serial handshake protocol** that differs significantly from standard joystick operation:

1. **Serial Communication**: Uses clock/data lines instead of parallel GPIO
2. **Latch Signal**: Amiga sends a LATCH pulse (high) to initiate a read cycle
3. **Clock Pulses**: Amiga sends CLOCK pulses to shift data out
4. **Data Response**: Controller responds with button data on DATA pin, one bit per clock pulse
5. **Button Sequence**: Shifts out buttons in order: Blue, Red, Yellow, Green, Right Trigger, Left Trigger, Start, 1, 0

### Button Mapping

CD32 supports 7 buttons plus D-pad:
- **Blue Button** (bit 0): First bit shifted out
- **Red Button** (bit 1)
- **Yellow Button** (bit 2)
- **Green Button** (bit 3)
- **Right Trigger** (bit 4)
- **Left Trigger** (bit 5)
- **Start Button** (bit 6)
- **D-pad UP/DOWN**: Direct GPIO outputs (not serial)

### Timing Requirements

- **Latch pulse**: ~10-20μs (Amiga initiates)
- **Clock pulses**: ~10-20μs each (Amiga controls timing)
- **Data setup time**: Must be stable before clock rising edge
- **Response time**: Must respond within microseconds

## Current Implementation Analysis

### Standard Joystick Protocol (Current)

**GPIO Usage (Port 1, Revision 5):**
- GPIO 10 (`QM1_AMIGA_V`): UP direction
- GPIO 11 (`QM1_AMIGA_H`): DOWN direction
- GPIO 12 (`QM1_AMIGA_VQ`): LEFT direction
- GPIO 13 (`QM1_AMIGA_HQ`): RIGHT direction
- GPIO 14 (`QM1_AMIGA_B1`): Fire button
- GPIO 2 (`QM1_AMIGA_B2`): Button 2
- GPIO 3 (`QM1_AMIGA_B3`): Button 3

**Characteristics:**
- Parallel GPIO: Each button/direction has its own pin
- Simple LOW/HIGH states
- No timing requirements
- Direct control via `amiga_gpio_set_active_low()`

### CD32 Protocol Requirements

**DB-9 Pin Mapping (from KTRL_CD32):**
- **Pin 2**: Clock (input from Amiga)
- **Pin 3**: Latch (input from Amiga)
- **Pin 4**: Data (output to Amiga)
- **Pin 7**: Fire1 (direct output, not serial)
- **Pin 9**: Data (alternative - some implementations use pin 9)
- **Pin 10**: Down (direct output for D-pad)
- **Pin 13**: Up (direct output for D-pad)

**GPIO Mapping Needed:**
- **Clock**: GPIO 2 (currently `QM1_AMIGA_B2`) - **CONFLICT!**
- **Latch**: GPIO 3 (currently `QM1_AMIGA_B3`) - **CONFLICT!**
- **Data**: GPIO 4 (currently `KBD_AMIGA_RST`) - **CONFLICT!**
- **Fire1**: GPIO 14 (currently `QM1_AMIGA_B1`) - **OK, can share**
- **Up**: GPIO 10 (currently `QM1_AMIGA_V`) - **OK, can share**
- **Down**: GPIO 11 (currently `QM1_AMIGA_H`) - **OK, can share**

## GPIO Pin Conflicts

### Critical Conflicts

1. **GPIO 2 (Clock)**: Currently used for Button 2
   - **Solution**: Mode-dependent GPIO configuration
   - **Impact**: Button 2 unavailable in CD32 mode

2. **GPIO 3 (Latch)**: Currently used for Button 3
   - **Solution**: Mode-dependent GPIO configuration
   - **Impact**: Button 3 unavailable in CD32 mode

3. **GPIO 4 (Data)**: Currently used for Keyboard Reset
   - **Solution**: Share pin, disable keyboard reset during CD32 mode
   - **Impact**: Keyboard reset unavailable during CD32 mode (acceptable)

### Compatible Pins

- **GPIO 10 (Up)**: Can be shared (D-pad UP in CD32, UP direction in standard)
- **GPIO 11 (Down)**: Can be shared (D-pad DOWN in CD32, DOWN direction in standard)
- **GPIO 14 (Fire1)**: Can be shared (Fire button in both modes)

## Implementation Approach

### Option 1: User-Selectable Mode (Recommended)

**Implementation:**
- Add mode selection via OLED menu or button sequence
- Store mode preference in EEPROM/flash
- Switch GPIO configuration based on mode
- Allow runtime switching (with brief reinitialization)

**Pros:**
- User control
- Maintains standard joystick compatibility
- Easy to test and debug

**Cons:**
- Requires user interaction to switch
- Can't auto-detect which mode game expects

### Option 2: Auto-Detection

**Implementation:**
- Monitor LATCH pin (GPIO 3) for activity
- If LATCH pulses detected, switch to CD32 mode
- Fall back to standard joystick if no LATCH activity for X seconds

**Pros:**
- Automatic mode switching
- No user interaction needed

**Cons:**
- More complex detection logic
- Potential false positives
- May switch modes unexpectedly

### Option 3: Port 1 Only (Initial Implementation)

**Implementation:**
- Implement CD32 on Port 1 only
- Keep Port 2 as standard joystick
- Allows testing without breaking existing functionality

**Pros:**
- Safe testing approach
- Doesn't affect Port 2
- Can be expanded later

**Cons:**
- Limited to one port
- May confuse users

## Code Structure

### New Files Needed

1. **`src/platform/amiga/cd32_pad.h`**
   - Function declarations
   - CD32 mode state management
   - Button mapping definitions

2. **`src/platform/amiga/cd32_pad.c`**
   - CD32 protocol implementation
   - Interrupt handlers
   - State machine
   - Button mapping logic

### Modified Files

1. **`src/config.h`**
   - Add CD32 GPIO pin definitions
   - Add mode selection configuration

2. **`src/platform/amiga/joystick_port1.c`**
   - Add mode detection/selection
   - Route button inputs to CD32 or standard joystick
   - Handle mode switching

3. **`src/usb_hid.c`**
   - Map gamepad buttons to CD32 button format
   - Handle CD32-specific button mappings

4. **`src/display/display.c`**
   - Add CD32 mode indicator to OLED
   - Add mode selection menu option

### Core Implementation Components

#### 1. Interrupt Handlers

```c
// Clock ISR - shift out next button bit
void cd32_clock_isr(uint gpio, uint32_t events) {
    // Shift out next button bit on DATA pin
    // Increment button index
}

// Latch ISR - reset to first button
void cd32_latch_isr(uint gpio, uint32_t events) {
    // Reset button index to 0 (Blue button)
    // Set DATA pin to Blue button state
}
```

#### 2. State Machine

```c
typedef struct {
    volatile uint8_t button_index;  // Current button being shifted (0-8)
    uint8_t button_states[9];       // Button states (active low)
    bool cd32_mode_active;          // CD32 mode enabled
    bool shifting;                   // Currently shifting data
} cd32_state_t;
```

#### 3. Button Mapping

```c
// Map USB/Bluetooth gamepad to CD32 buttons
void cd32_map_gamepad_buttons(uni_gamepad_t* gamepad, cd32_state_t* state) {
    state->button_states[0] = !(gamepad->buttons & BUTTON_B);      // Blue
    state->button_states[1] = !(gamepad->buttons & BUTTON_A);      // Red
    state->button_states[2] = !(gamepad->buttons & BUTTON_Y);      // Yellow
    state->button_states[3] = !(gamepad->buttons & BUTTON_X);       // Green
    state->button_states[4] = !(gamepad->buttons & BUTTON_SHOULDER_R); // R Trigger
    state->button_states[5] = !(gamepad->buttons & BUTTON_SHOULDER_L); // L Trigger
    state->button_states[6] = !(gamepad->buttons & BUTTON_MISC_BACK);  // Start
    state->button_states[7] = 1;  // Always 1
    state->button_states[8] = 0;  // Always 0
}
```

#### 4. GPIO Configuration

```c
void cd32_init_gpio(void) {
    // Configure Clock pin (GPIO 2) as INPUT with pull-up
    gpio_set_function(CD32_CLOCK_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(CD32_CLOCK_PIN, GPIO_IN);
    gpio_set_pulls(CD32_CLOCK_PIN, true, false);
    
    // Configure Latch pin (GPIO 3) as INPUT with pull-up
    gpio_set_function(CD32_LATCH_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(CD32_LATCH_PIN, GPIO_IN);
    gpio_set_pulls(CD32_LATCH_PIN, true, false);
    
    // Configure Data pin (GPIO 4) as OUTPUT
    gpio_set_function(CD32_DATA_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(CD32_DATA_PIN, GPIO_OUT);
    gpio_put(CD32_DATA_PIN, 1);  // Default HIGH (inactive)
    
    // Setup interrupt handlers
    gpio_set_irq_enabled_with_callback(CD32_CLOCK_PIN, GPIO_IRQ_EDGE_RISE, true, &cd32_clock_isr);
    gpio_set_irq_enabled_with_callback(CD32_LATCH_PIN, GPIO_IRQ_EDGE_RISE, true, &cd32_latch_isr);
}
```

## Implementation Steps

### Phase 1: Preparation (2-4 hours)

1. **Research and Documentation**
   - Review KTRL_CD32 code in detail
   - Document exact timing requirements
   - Verify DB-9 pin mappings
   - Test GPIO pin availability

2. **Design Decisions**
   - Choose mode selection approach (user-selectable vs auto-detect)
   - Design button mapping scheme
   - Plan GPIO conflict resolution

### Phase 2: Core Implementation (8-12 hours)

1. **Create CD32 Module**
   - Create `cd32_pad.h` and `cd32_pad.c`
   - Implement state machine structure
   - Implement interrupt handlers
   - Implement button mapping logic

2. **GPIO Configuration**
   - Add CD32 GPIO definitions to `config.h`
   - Implement mode-dependent GPIO setup
   - Handle GPIO conflicts

3. **Integration with Port 1**
   - Modify `joystick_port1.c` for mode switching
   - Route button inputs to CD32 or standard joystick
   - Handle mode initialization

### Phase 3: User Interface (2-4 hours)

1. **OLED Menu**
   - Add CD32 mode indicator
   - Add mode selection menu option
   - Display current mode on splash screen

2. **Button Mapping UI** (Optional)
   - Allow user to customize button mappings
   - Store preferences in EEPROM

### Phase 4: Testing and Debugging (4-8 hours)

1. **Hardware Testing**
   - Test on actual Amiga hardware
   - Verify timing requirements
   - Test with CD32 games
   - Test mode switching

2. **Compatibility Testing**
   - Test with standard joystick games (should still work)
   - Test with CD32 games
   - Test with different gamepads

3. **Edge Cases**
   - Test rapid mode switching
   - Test with no gamepad connected
   - Test with multiple gamepads

## Risks and Considerations

### Technical Risks

1. **Timing Sensitivity**
   - CD32 protocol is timing-critical
   - RP2040 should handle it, but needs careful implementation
   - May need to disable interrupts during critical sections

2. **GPIO Conflicts**
   - Keyboard reset pin (GPIO 4) conflict
   - Button 2/3 pins conflict
   - Need careful mode switching

3. **Core 1 Interference**
   - Mouse quadrature runs on Core 1
   - CD32 protocol runs on Core 0
   - Should be less conflict than current joystick/mouse sharing

### Compatibility Risks

1. **Standard Joystick Games**
   - Games expecting standard joystick won't work in CD32 mode
   - Need clear mode indication
   - Need easy mode switching

2. **Amiga Model Differences**
   - CD32 protocol may vary slightly between models
   - Need testing on multiple Amiga models

### User Experience Risks

1. **Mode Confusion**
   - Users may not understand when to use CD32 mode
   - Need clear documentation
   - Need intuitive mode selection

2. **Button Mapping**
   - CD32 button names (Blue/Red/Yellow/Green) may confuse users
   - Need clear mapping documentation

## Advantages of CD32 Protocol

1. **Solves Voltage Issues**
   - Uses serial communication, not voltage levels
   - No voltage threshold problems
   - More reliable across Amiga models

2. **More Buttons**
   - Supports 7 buttons + D-pad
   - Better for modern gamepads
   - More compatible with CD32 software

3. **Better Protocol**
   - Designed for gamepads, not simple joysticks
   - More robust communication
   - Less susceptible to electrical issues

## Disadvantages of CD32 Protocol

1. **Compatibility**
   - Breaks compatibility with standard joystick games
   - Not all Amiga models support CD32 protocol
   - Requires mode switching

2. **Complexity**
   - More complex than standard joystick
   - Timing-sensitive code
   - More potential for bugs

3. **GPIO Conflicts**
   - Conflicts with existing pin usage
   - Requires careful mode management
   - May affect keyboard functionality

## Testing Plan

### Unit Testing

1. **State Machine**
   - Test button index tracking
   - Test state transitions
   - Test edge cases

2. **Interrupt Handlers**
   - Test clock ISR timing
   - Test latch ISR timing
   - Test data shifting

3. **Button Mapping**
   - Test all button mappings
   - Test edge cases (no gamepad, multiple gamepads)

### Integration Testing

1. **Mode Switching**
   - Test standard joystick → CD32 mode
   - Test CD32 → standard joystick mode
   - Test rapid switching

2. **GPIO Conflicts**
   - Test keyboard reset during CD32 mode
   - Test button 2/3 behavior
   - Test mode-dependent GPIO configuration

### Hardware Testing

1. **Amiga Compatibility**
   - Test on A500, A600, A1200, A2000
   - Test with CD32 console
   - Test with CD32 games

2. **Gamepad Compatibility**
   - Test with DS5, Stadia, Xbox controllers
   - Test with USB gamepads
   - Test button mapping accuracy

## Estimated Timeline

- **Phase 1 (Preparation)**: 2-4 hours
- **Phase 2 (Core Implementation)**: 8-12 hours
- **Phase 3 (User Interface)**: 2-4 hours
- **Phase 4 (Testing)**: 4-8 hours
- **Total**: 16-28 hours

## Recommendation

**Priority: MEDIUM**

1. **First**: Fix grounding issues (likely solves 400-500mV problem)
2. **Then**: If grounding doesn't fully resolve, consider CD32 as alternative
3. **Implementation**: Start with Option 1 (user-selectable mode) on Port 1 only
4. **Testing**: Thorough testing on multiple Amiga models before full release

## Next Steps

1. Review this implementation plan
2. Decide on mode selection approach
3. Resolve GPIO pin conflicts
4. Create detailed technical specification
5. Begin Phase 1 implementation

## Notes

- CD32 protocol implementation is feasible but requires significant effort
- KTRL_CD32 code provides excellent reference implementation
- RP2040 has sufficient performance for timing requirements
- Main challenge is GPIO conflicts and mode switching logic
- Consider implementing as optional feature initially, not default

