# Original GPIO Configuration (Main Branch)

## Summary

The original code in the main branch used a **very simple GPIO configuration**:

### For Active (LOW) State:
```c
gpio_put(gpio, 0);           // Drive LOW
gpio_set_dir(gpio, GPIO_OUT); // Set to OUTPUT
```

### For Inactive (HIGH) State:
```c
gpio_set_dir(gpio, GPIO_IN);  // Set to INPUT
// NO pull-up configuration - relies on Amiga's pull-ups
```

## Original Implementation

The original code used inline functions:

### Mouse/Joystick (`_aqm_gpio_set`):
```c
static inline void _aqm_gpio_set(uint gpio, enum _mouse_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }
    
    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}
```

### Keyboard (`_keyboard_gpio_set`):
```c
static inline void _keyboard_gpio_set(uint gpio, enum _keyboard_pin_state state)
{
    if (state == LOW) {
        gpio_put(gpio, 0);
        gpio_set_dir(gpio, GPIO_OUT);
        return;
    }
    
    // assume it's high otherwise
    gpio_set_dir(gpio, GPIO_IN);
}
```

## Key Observations

1. **No pull-up configuration**: The original code did NOT configure pull-ups on the Pico side
2. **Relied on Amiga's pull-ups**: When inactive, GPIO was set to INPUT mode and relied entirely on the Amiga's internal pull-ups (~10kΩ) to pull the signal HIGH
3. **Simple and direct**: This approach works perfectly when directly connected to the Amiga (no level shifter)

## Comparison with Current Code

### Current Code (with `ENABLE_LEVEL_SHIFTER`):
```c
if (active) {
    // Active: Drive LOW (0V) as OUTPUT
    gpio_set_pulls(gpio, false, false);  // Disable pulls
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
} else {
    // Inactive: Set as INPUT with NO pull-up
    gpio_set_dir(gpio, GPIO_IN);
    gpio_set_pulls(gpio, false, false);  // No pull-up - level shifter handles it
}
```

### Current Code (without level shifter):
```c
if (active) {
    // Active: Drive LOW (0V) as OUTPUT
    gpio_set_pulls(gpio, false, false);  // Disable pulls
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
} else {
    // Inactive: Set as INPUT with pull-up (3.3V)
    gpio_set_dir(gpio, GPIO_IN);
    gpio_set_pulls(gpio, true, false);  // Enable pull-up, disable pull-down
}
```

## Conclusion

**The original code matches our current level shifter configuration!**

- Original: INPUT mode, no pull-up (relies on Amiga's pull-ups)
- Current (with level shifter): INPUT mode, no pull-up (relies on level shifter + Amiga's pull-ups)

The difference is:
- **Original**: Direct connection to Amiga, Amiga's pull-ups work directly
- **Current (with level shifter)**: TXB0108 level shifter in between, requires external pull-ups on 5V side for proper operation

This confirms that our current approach is correct - we're using the same GPIO configuration as the original code, but the TXB0108 level shifter requires additional hardware support (external pull-ups) to properly translate the signals.

