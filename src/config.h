/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * https://github.com/borb/amigahid-pico
 *
 * Modifications Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * hid-pico configuration
 *
 * PLEASE NOTE ALL PIN DESIGNATIONS ARE GPIO PIN NUMBERS - NOT PHYSICAL PIN NUMBERS
 */

#ifndef _CONFIG_H
#define _CONFIG_H

// Software version - single source of truth for main.c, OLED display, and serial output.
// v3.0.0: Rev 5/6 GPIO maps + Core 1 loop-counter mouse consume (Stadia-safe).
// v3.2.0: USB device mode — read a real Amiga keyboard/mouse and present as a USB HID to a PC.
// v4.0.0: Public release packaging — ultrausbt branding, LICENSE/NOTICE, quiet UART, docs.
// v4.0.1: Llamatron twin-stick allowed with Port 2 CD32 (still exclusive with Port 1 CD32).
// v4.0.2: Splash shows both port modes; Right=Port2, Middle+Left=BT pairing.
// v4.0.3: Port1 cycle includes Ami/Atari mouse; Port2 Joy↔CD32.
// v4.0.4: Splash "Controller Mode" heading; Ami Ms / Atr Ms labels; Port1 above Port2.
#ifndef SOFTWARE_VERSION_MAJOR
#  define SOFTWARE_VERSION_MAJOR 4
#endif
#ifndef SOFTWARE_VERSION_MINOR
#  define SOFTWARE_VERSION_MINOR 0
#endif
#ifndef SOFTWARE_VERSION_PATCH
#  define SOFTWARE_VERSION_PATCH 4
#endif

// USB device mode ("PC keyboard" mode): read a real Amiga keyboard (and mouse) on the
// keyboard connector / Port 1 and present the adapter to a PC as a USB HID keyboard+mouse.
// This is the reverse of normal operation. The two roles share one USB PHY, so only one is
// active at a time; the mode is chosen at boot from flash and toggled at runtime via the OLED.
#ifndef ENABLE_USB_DEVICE_MODE
#  define ENABLE_USB_DEVICE_MODE 1
#endif

// USB HID device identity presented to the host PC (Raspberry Pi VID + project PID).
#ifndef USB_DEVICE_VID
#  define USB_DEVICE_VID 0x2E8A
#endif
#ifndef USB_DEVICE_PID
#  define USB_DEVICE_PID 0xAB1A         /* "ABIA" ~ amiga */
#endif

// Log Amiga keyboard receive (frames, keycodes, 1 Hz KCLK heartbeat, resync) on UART.
// Default off for quiet builds; set to 1 when diagnosing device-mode keyboard issues.
// See doc/archive/amiga-usb-device-mode.md ("Diagnosing with KEYBOARD_IN_DEBUG").
#ifndef KEYBOARD_IN_DEBUG
#  define KEYBOARD_IN_DEBUG 0
#endif

// Verbose controller / HID report dumps (hex dumps, periodic stick samples).
// Mount/unmount and Bluepad32 connect messages stay on regardless. Also enable
// with CMake: add_compile_definitions(DEBUG_MESSAGES=1) for ahprintf + DIAG.
#ifndef CONTROLLER_DEBUG
#  define CONTROLLER_DEBUG 0
#endif

// Bluetooth gamepad pairing — Core 1 pause timing (Atari v22.1.0 / ultramegausb family)
#ifndef BT_GAMEPAD_DISCOVERY_SETTLE_MS
#  define BT_GAMEPAD_DISCOVERY_SETTLE_MS 30
#endif
#ifndef BT_GAMEPAD_CORE1_RESUME_DELAY_MS
#  define BT_GAMEPAD_CORE1_RESUME_DELAY_MS 100
#endif
// Force-release BT Core 1 pause if enumeration aborts without disconnect callback
#ifndef BT_CORE1_PAUSE_WATCHDOG_MS
#  define BT_CORE1_PAUSE_WATCHDOG_MS 45000
#endif
// Pause Core 1 during BLE gamepad discovery (Atari v22.1.0 default).
#ifndef BT_PAUSE_CORE1_ON_GAMEPAD_DISCOVERY
#  define BT_PAUSE_CORE1_ON_GAMEPAD_DISCOVERY 1
#endif
// If Core 1 heartbeat does not advance for this long (and not intentionally
// paused), attempt SEV wake then relaunch the quadrature loop.
#ifndef CORE1_HEARTBEAT_STALL_MS
#  define CORE1_HEARTBEAT_STALL_MS 500
#endif

// Remap one HID scancode to Amiga numpad * (AMIGA_KPAST / 0x5d — PrtScn on Amiga layout).
// Set HID code to 0 to disable. Logitech MX "| / ~ #" key → HID 0x32 (UART verified).
#ifndef KEY_REMAP_HID_TO_HELP
#  define KEY_REMAP_HID_TO_HELP  0x32
#endif

// Log HID scancodes on UART when keys are pressed (set 1 to discover remaps).
// Output: `[kbd] HID 0xNN -> Amiga 0xNN` per key down. See doc/archive/device_troubleshooting.md.
#ifndef KEYBOARD_HID_DEBUG
#  define KEYBOARD_HID_DEBUG  0
#endif

// Log the hard-reset key combo state machine on UART (Ctrl+LAmiga+RAmiga/Backspace).
// Prints ctrl/lamiga/ramiga/backspace/combo/in_reset whenever a tracked key changes.
#ifndef KEYBOARD_RESET_DEBUG
#  define KEYBOARD_RESET_DEBUG  0
#endif

// Minimum time (ms) to hold the Amiga reset line low once the combo is detected.
// Guarantees a real reset even if a keyboard drops a combo key immediately
// (matrix ghosting, e.g. MX Keys Mini dropping Backspace on the third key).
#ifndef RESET_ASSERT_MIN_HOLD_MS
#  define RESET_ASSERT_MIN_HOLD_MS  500
#endif

#ifndef HIDPICO_REVISION
#  define HIDPICO_REVISION 4
#endif

// Level shifter configuration
// Set to 1 if using level shifters (5V ↔ 3.3V) on joystick GPIOs
// When enabled, Pico's internal pull-ups are disabled for inactive signals
// to avoid conflicts with level shifter direction detection and 5V-side pull-ups
// 
// IMPORTANT: If you experience delayed input or signals only working when fire
// button is pressed, ensure this is set to 1 and that your level shifter has
// proper pull-ups on the 5V side (or rely on Amiga's internal pull-ups)
#ifndef ENABLE_LEVEL_SHIFTER
#  if HIDPICO_REVISION == 6
#    define ENABLE_LEVEL_SHIFTER 0  // Rev 6: direct 5V on RP2350 GPIO 0-25
#  else
#    define ENABLE_LEVEL_SHIFTER 1  // Rev 5 and earlier: level shifters on joystick GPIOs
#  endif
#endif

// the pico has an onboard led on gp25; use this as a default indicator
#ifndef INDICATOR_LED
#  define INDICATOR_LED PICO_DEFAULT_LED_PIN
#endif

#if HIDPICO_REVISION == 2
#  define I2C_PORT      i2c0
#  define I2C_PIN_SDA   4
#  define I2C_PIN_SCL   5
#  define I2C_IRQN      23

#  define KBD_AMIGA_RST 10
#  define KBD_AMIGA_DAT 11
#  define KBD_AMIGA_CLK 12

#  define QM1_AMIGA_HQ  7
#  define QM1_AMIGA_VQ  6
#  define QM1_AMIGA_H   9
#  define QM1_AMIGA_V   8
#  define QM1_AMIGA_B1  22
#  define QM1_AMIGA_B2  26
#  define QM1_AMIGA_B3  27
#elif HIDPICO_REVISION == 4
#  define I2C_PORT      i2c1
#  define I2C_PIN_SDA   2
#  define I2C_PIN_SCL   3
#  define I2C_IRQN      24 // this is tied to the i2c port being used so get the right one!

#  define KBD_AMIGA_RST 4
#  define KBD_AMIGA_DAT 5
#  define KBD_AMIGA_CLK 6

#  define QM1_AMIGA_HQ  7
#  define QM1_AMIGA_VQ  8
#  define QM1_AMIGA_H   9
#  define QM1_AMIGA_V   10
#  define QM1_AMIGA_B1  11
#  define QM1_AMIGA_B2  12
#  define QM1_AMIGA_B3  13
#elif HIDPICO_REVISION == 5
#  define I2C_PORT      i2c0   // Changed to i2c0 to match Atari board (GPIO 8/9 are on I2C0)
#  define I2C_PIN_SDA   8   // Physical pin 11 (GP8) - Atari board I2C SDA
#  define I2C_PIN_SCL   9   // Physical pin 12 (GP9) - Atari board I2C SCL
#  define I2C_IRQN      23  // Changed to 23 for I2C0 (was 24 for I2C1)

#  define KBD_AMIGA_RST 4
#  define KBD_AMIGA_DAT 5
#  define KBD_AMIGA_CLK 6

// Joystick Port 1 GPIO mappings (mapped to Atari JOY1 GPIOs for hardware compatibility)
// Directions and FIRE use Atari JOY1 GPIO pins
#  define JOY1_ATARI_UP    10  // Atari JOY1 UP GPIO
#  define JOY1_ATARI_DOWN  11  // Atari JOY1 DOWN GPIO
#  define JOY1_ATARI_LEFT  12  // Atari JOY1 LEFT GPIO
#  define JOY1_ATARI_RIGHT 13  // Atari JOY1 RIGHT GPIO
#  define JOY1_ATARI_FIRE  14  // Atari JOY1 FIRE GPIO

// Port 1 direction pins (mapped to Atari JOY1 GPIOs)
#  define QM1_AMIGA_V    JOY1_ATARI_UP     // UP direction - Atari JOY1 UP
#  define QM1_AMIGA_H    JOY1_ATARI_DOWN   // DOWN direction - Atari JOY1 DOWN
#  define QM1_AMIGA_VQ   JOY1_ATARI_LEFT   // LEFT direction - Atari JOY1 LEFT
#  define QM1_AMIGA_HQ   JOY1_ATARI_RIGHT  // RIGHT direction - Atari JOY1 RIGHT

// Port 1 button pins
#  define QM1_AMIGA_B1   JOY1_ATARI_FIRE   // Fire button - Atari JOY1 FIRE
#  define QM1_AMIGA_B2   2                 // Button 2 - Remapped to GPIO 2 (no conflicts)
#  define QM1_AMIGA_B3   3                 // Button 3 - Remapped to GPIO 3 (no conflicts)

// Joystick Port 2 GPIO mappings (mapped to Atari JOY0 GPIOs for hardware compatibility)
// Directions and FIRE use Atari JOY0 GPIO pins
#  define JOY0_ATARI_UP    19  // Atari JOY0 UP GPIO
#  define JOY0_ATARI_DOWN  20  // Atari JOY0 DOWN GPIO
#  define JOY0_ATARI_LEFT  21  // Atari JOY0 LEFT GPIO
#  define JOY0_ATARI_RIGHT 22  // Atari JOY0 RIGHT GPIO
#  define JOY0_ATARI_FIRE  26  // Atari JOY0 FIRE GPIO

// Port 2 direction pins (mapped to Atari JOY0 GPIOs)
#  define QM2_AMIGA_V    JOY0_ATARI_UP     // UP direction - Atari JOY0 UP
#  define QM2_AMIGA_H    JOY0_ATARI_DOWN   // DOWN direction - Atari JOY0 DOWN
#  define QM2_AMIGA_VQ   JOY0_ATARI_LEFT   // LEFT direction - Atari JOY0 LEFT
#  define QM2_AMIGA_HQ   JOY0_ATARI_RIGHT  // RIGHT direction - Atari JOY0 RIGHT

// Port 2 button pins
#  define QM2_AMIGA_B1   JOY0_ATARI_FIRE   // Fire button - Atari JOY0 FIRE
#  define QM2_AMIGA_B2   27                 // Button 2 - GPIO 27 (ADC1)
#  define QM2_AMIGA_B3   28                 // Button 3 - Remapped to GPIO 28 (no conflicts)

// SSD1306 OLED Display configuration (matches Atari board)
#  define SSD1306_SDA    I2C_PIN_SDA       // GPIO 8 (same as I2C_PIN_SDA)
#  define SSD1306_SCL    I2C_PIN_SCL       // GPIO 9 (same as I2C_PIN_SCL)
#  define SSD1306_I2C    I2C_PORT          // i2c0
#  define SSD1306_ADDR   0x3c               // I2C address
#  define SSD1306_WIDTH  128                // Display width in pixels
#  define SSD1306_HEIGHT 64                 // Display height in pixels

// GPIO assignments for UI buttons (matches Atari board)
#  define GPIO_BUTTON_LEFT   18             // Left button (Port 1 mode; with right: hold 5s clears BT keys)
#  define GPIO_BUTTON_MIDDLE 17             // Center button (toggle screens)
#  define GPIO_BUTTON_RIGHT  16             // Right button (toggle BT pairing on splash)
#elif HIDPICO_REVISION == 6
// Rev 6: direct 5V on RP2350 (Pico 2) without level shifters.
// OLED UI buttons move to ADC pins 26-28 (3.3V only, no Amiga 5V).
// Port 2 fire/B2/B3 use freed header GPIOs 16-18 (5V-tolerant).
// GPIO 0/1 reserved for debug UART. See doc/gpio_rev6_adc_avoidance.md.
#  define I2C_PORT      i2c0
#  define I2C_PIN_SDA   8
#  define I2C_PIN_SCL   9
#  define I2C_IRQN      23

#  define KBD_AMIGA_RST 4
#  define KBD_AMIGA_DAT 5
#  define KBD_AMIGA_CLK 6

#  define JOY1_ATARI_UP    10
#  define JOY1_ATARI_DOWN  11
#  define JOY1_ATARI_LEFT  12
#  define JOY1_ATARI_RIGHT 13
#  define JOY1_ATARI_FIRE  14

#  define QM1_AMIGA_V    JOY1_ATARI_UP
#  define QM1_AMIGA_H    JOY1_ATARI_DOWN
#  define QM1_AMIGA_VQ   JOY1_ATARI_LEFT
#  define QM1_AMIGA_HQ   JOY1_ATARI_RIGHT
#  define QM1_AMIGA_B1   JOY1_ATARI_FIRE
#  define QM1_AMIGA_B2   2                 // 5V-tolerant (unchanged from Rev 5)
#  define QM1_AMIGA_B3   3                 // 5V-tolerant (unchanged from Rev 5)

#  define JOY0_ATARI_UP    19
#  define JOY0_ATARI_DOWN  20
#  define JOY0_ATARI_LEFT  21
#  define JOY0_ATARI_RIGHT 22
#  define JOY0_ATARI_FIRE  16                 // Was GPIO 26 (ADC0) on Rev 5

#  define QM2_AMIGA_V    JOY0_ATARI_UP
#  define QM2_AMIGA_H    JOY0_ATARI_DOWN
#  define QM2_AMIGA_VQ   JOY0_ATARI_LEFT
#  define QM2_AMIGA_HQ   JOY0_ATARI_RIGHT
#  define QM2_AMIGA_B1   JOY0_ATARI_FIRE      // Fire — GPIO 16 (5V-tolerant)
#  define QM2_AMIGA_B2   17                   // Was GPIO 27 (ADC1); CD32 DATA
#  define QM2_AMIGA_B3   18                   // Was GPIO 28 (ADC2); CD32 JOYMODE

#  define DEBUG_UART_TX  0
#  define DEBUG_UART_RX  1

#  define SSD1306_SDA    I2C_PIN_SDA
#  define SSD1306_SCL    I2C_PIN_SCL
#  define SSD1306_I2C    I2C_PORT
#  define SSD1306_ADDR   0x3c
#  define SSD1306_WIDTH  128
#  define SSD1306_HEIGHT 64

// OLED UI buttons on ADC pins — 3.3V tactile switches only (not 5V-tolerant lines)
#  define GPIO_BUTTON_LEFT   26             // Was GPIO 18 on Rev 5
#  define GPIO_BUTTON_MIDDLE 27             // Was GPIO 17 on Rev 5
#  define GPIO_BUTTON_RIGHT  28             // Was GPIO 16 on Rev 5
#else
#  error "HIDPICO_REVISION must be 2, 4, 5, or 6. Current value is not recognized."
#endif

// Rev 5 and Rev 6 share ultramegausb Atari-board firmware (OLED, CD32, dual-port).
#if HIDPICO_REVISION == 5 || HIDPICO_REVISION == 6
#  define HIDPICO_REV_ATARI_BOARD 1
#endif

#endif // _CONFIG_H
