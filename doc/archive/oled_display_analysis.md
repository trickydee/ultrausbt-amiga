# OLED Display Library Analysis

## Current Amiga Project Implementation

### Library Used
- **Custom implementation**: `src/display/disp_ssd.c`
- **Graphics library**: UGUI (`src/display/ugui.c` and `ugui.h`)
- **Architecture**: DMA-based I2C transfers with interrupt-driven transaction queue

### Current Display Usage

The OLED screen is currently used for **debug information only**:

1. **Line 0 (top)**: USB device counters
   - Format: `"usb    k:%02x m:%02x j:%02x"`
   - Shows count of connected USB keyboards, mice, and joysticks
   - Updated via `dbgcons_print_counters()` in `src/util/debug_cons.c`

2. **Line 1 (bottom)**: Keyboard input debug
   - Format: `"amikb hid:%02x ami:%02x %s"`
   - Shows HID keycode, Amiga keycode, and up/down status
   - Updated via `dbgcons_amiga_key()` in `src/util/debug_cons.c`

### Current API

**Function pointer interface:**
```c
void (*disp_write)(uint8_t x, uint8_t y, char *message);
```

**Implementation details:**
- `x` and `y` are character positions (not pixels)
- Uses UGUI library for text rendering
- Character size: 5x12 pixels (with 16-pixel line spacing)
- Only supports 2 lines of text (y=0 and y=1)
- Automatic screen update after each write

### Technical Details

**I2C Configuration:**
- Address: `0x3c` (hardcoded)
- Speed: 1MHz (very fast)
- Uses DMA channels for transfers
- Interrupt-driven transaction queue
- Supports up to 1KB + 32B transfers

**Display Specifications:**
- Width: 128 pixels
- Height: 64 pixels
- Framebuffer: `(128 * 64) / 8 = 1024 bytes`

**Initialization:**
- Called in `main.c` via `disp_ssd_init()`
- Falls back to no-op if I2C init fails
- Uses complex initialization sequence with many SSD1306 commands

### Files Involved

1. **`src/display/disp_ssd.c`** - Main display driver (616 lines)
2. **`src/display/disp_ssd.h`** - Header with function pointer declaration
3. **`src/display/ugui.c`** - UGUI graphics library
4. **`src/display/ugui.h`** - UGUI header
5. **`src/util/debug_cons.c`** - Debug console that writes to display (2 calls to `disp_write`)

### Current Limitations

1. **Limited to 2 lines of text** - Only y=0 and y=1 are used
2. **Fixed character size** - 5x12 pixels, no scaling
3. **Character position only** - No pixel-level control
4. **Debug-only usage** - Not used for user-facing information
5. **Complex implementation** - DMA, interrupts, transaction queue (overkill for simple text)

---

## Atari Project Implementation

### Library Used
- **Library**: `ssd1306` by David Schramm (MIT License)
- **Location**: `ssd1306/ssd1306.c` and `ssd1306/ssd1306.h`
- **Architecture**: Simple blocking I2C writes (no DMA/interrupts)

### Display Usage

The OLED is used extensively throughout the Atari project for:

1. **User interface messages** - Status, errors, controller info
2. **Controller initialization** - Xbox, PS3, PS4, Switch, Stadia status
3. **Debug information** - Button states, analog values
4. **Multi-language support** - User interface in multiple languages

### API

**Main functions:**
```c
// Initialization
bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, 
                  uint8_t address, i2c_inst_t *i2c_instance);

// Display control
void ssd1306_clear(ssd1306_t *p);
void ssd1306_show(ssd1306_t *p);
void ssd1306_poweron(ssd1306_t *p);
void ssd1306_poweroff(ssd1306_t *p);

// Drawing functions
void ssd1306_draw_string(ssd1306_t *p, int x, int y, int scale, const char *s);
void ssd1306_draw_char(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, char c);
void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y);
void ssd1306_draw_line(ssd1306_t *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void ssd1306_draw_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
```

### Key Features

1. **Flexible positioning** - Pixel-level x,y coordinates
2. **Text scaling** - Scale parameter (1, 2, 3, etc.) for larger text
3. **Simple API** - Easy to use, no function pointers
4. **Blocking I2C** - Simpler, but may block during writes
5. **Built-in font** - Includes font rendering
6. **Graphics primitives** - Lines, squares, pixels

### Initialization Example (Atari)

```c
// In UserInterface.cpp
ssd1306_t disp;  // Global display instance

void UserInterface::init() {
    // Setup I2C
    i2c_init(SSD1306_I2C, 400000);
    gpio_set_function(SSD1306_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_SDA);
    gpio_pull_up(SSD1306_SCL);
    
    // Initialize display
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_ADDR, SSD1306_I2C);
}
```

### Usage Example (Atari)

```c
// Clear and show status
ssd1306_clear(&disp);
ssd1306_draw_string(&disp, 20, 10, 2, (char*)"XBOX");  // x=20, y=10, scale=2
ssd1306_draw_string(&disp, 5, 35, 1, (char*)"Ready!");
ssd1306_show(&disp);  // Update display
```

### Files in Atari Project

1. **`ssd1306/ssd1306.c`** - Library implementation (~217 lines)
2. **`ssd1306/ssd1306.h`** - Library header
3. **`ssd1306/font.h`** - Built-in font data
4. **`src/UserInterface.cpp`** - Main UI code that uses display
5. **Multiple controller files** - All use `extern ssd1306_t disp;`

---

## Comparison Summary

| Feature | Amiga (Current) | Atari (Target) |
|---------|----------------|----------------|
| **Library** | Custom + UGUI | ssd1306 (David Schramm) |
| **I2C Method** | DMA + Interrupts | Blocking writes |
| **Complexity** | High (616 lines) | Low (~217 lines) |
| **API Style** | Function pointer | Direct function calls |
| **Text Positioning** | Character-based (x,y) | Pixel-based (x,y) |
| **Text Scaling** | No | Yes (1x, 2x, 3x, etc.) |
| **Graphics Support** | UGUI (full graphics) | Basic primitives |
| **Current Usage** | Debug only (2 lines) | User interface + debug |
| **Flexibility** | Limited | High |

---

## Migration Considerations

### Advantages of Switching to Atari's Library

1. **Simpler codebase** - ~400 lines less code
2. **Easier to maintain** - Well-documented, standard library
3. **More flexible** - Pixel positioning, text scaling
4. **Consistent with Atari project** - Same library across both adapters
5. **Better for user interface** - Can display more information
6. **No DMA complexity** - Simpler I2C handling

### Potential Issues

1. **Blocking I2C** - May cause slight delays during display updates
2. **No interrupt-driven queue** - But probably not needed for simple text
3. **Need to rewrite display calls** - Current `disp_write()` calls need conversion
4. **I2C speed** - Atari uses 400kHz vs Amiga's 1MHz (but probably fine)

### Migration Steps (When Ready)

1. **Copy library files** from Atari project:
   - `ssd1306/ssd1306.c`
   - `ssd1306/ssd1306.h`
   - `ssd1306/font.h`

2. **Update CMakeLists.txt**:
   - Remove `ugui.c` from display sources
   - Add `ssd1306.c` to sources

3. **Replace initialization**:
   - Replace `disp_ssd_init()` with `ssd1306_init()`
   - Initialize I2C manually (like Atari does)

4. **Update display calls**:
   - Replace `disp_write(0, 0, "text")` with:
     ```c
     ssd1306_clear(&disp);
     ssd1306_draw_string(&disp, 0, 0, 1, "text");
     ssd1306_show(&disp);
     ```

5. **Remove old files**:
   - `src/display/disp_ssd.c`
   - `src/display/disp_ssd.h`
   - `src/display/ugui.c`
   - `src/display/ugui.h`

6. **Update includes**:
   - Replace `#include "display/disp_ssd.h"` with `#include "ssd1306.h"`

---

## Current Display Content

### What's Currently Shown

**Line 0 (top):**
```
usb    k:00 m:00 j:00
```
- USB keyboard count (hex)
- USB mouse count (hex)
- USB joystick count (hex)

**Line 1 (bottom):**
```
amikb hid:00 ami:00 up
```
- HID keycode (hex)
- Amiga keycode (hex)
- Key state (up/down)

### Potential Future Usage (After Migration)

With the Atari library, you could display:
- **Status messages** - "Ready", "Pairing", "Connected"
- **Controller info** - "DS5 Connected", "Xbox Ready"
- **Error messages** - "Pairing Failed", "Device Error"
- **Multi-line info** - Multiple lines of status
- **Larger text** - Scale 2x for important messages
- **Graphics** - Simple icons or status indicators

---

## Recommendation

**Yes, migrate to the Atari's ssd1306 library** because:

1. ✅ **Simpler** - Much less code to maintain
2. ✅ **More flexible** - Better for future UI features
3. ✅ **Consistent** - Same library as Atari project
4. ✅ **Well-tested** - Already proven in Atari project
5. ✅ **Standard** - Common library, well-documented

The current custom implementation is over-engineered for the simple debug text being displayed. The Atari library is more appropriate for the use case.

