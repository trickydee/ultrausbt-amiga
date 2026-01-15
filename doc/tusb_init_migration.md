# TinyUSB Init Migration Guide

## Issue

The `tuh_init()` function is deprecated in newer versions of TinyUSB and should be replaced with `tusb_init()`. However, the replacement requires a different API signature.

## Problem

When we tried to replace `tuh_init(BOARD_TUH_RHPORT)` with `tusb_init(BOARD_TUH_RHPORT, NULL)`, USB devices stopped working. This is because `tusb_init()` requires a proper initialization structure when called with two parameters.

## Solution

The correct way to use `tusb_init()` for host initialization is:

```c
#include "tusb.h"

// Initialize host stack on configured roothub port
tusb_rhport_init_t host_init = {
  .role = TUSB_ROLE_HOST,
  .speed = TUSB_SPEED_AUTO
};
tusb_init(BOARD_TUH_RHPORT, &host_init);
```

## Structure Definition

The `tusb_rhport_init_t` structure is defined in `tinyusb/src/common/tusb_types.h`:

```c
typedef struct {
  tusb_role_t role;    // TUSB_ROLE_HOST or TUSB_ROLE_DEVICE
  tusb_speed_t speed;  // TUSB_SPEED_AUTO, TUSB_SPEED_FULL, or TUSB_SPEED_HIGH
} tusb_rhport_init_t;
```

## Speed Options

- `TUSB_SPEED_AUTO` - Automatically detect and use the appropriate speed (recommended)
- `TUSB_SPEED_FULL` - Force Full Speed (12 Mbps)
- `TUSB_SPEED_HIGH` - Force High Speed (480 Mbps) - only if hardware supports it

## Current Status

For now, we're keeping `tuh_init()` as it works correctly. When ready to migrate, use the structure-based initialization shown above.

## References

- TinyUSB examples: `tinyusb/examples/host/hid_controller/src/main.c`
- TinyUSB source: `tinyusb/src/host/usbh.h` (line 160 shows deprecated wrapper)
- TinyUSB source: `tinyusb/src/tusb.c` (line 69 shows internal implementation)

