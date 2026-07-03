/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
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
// Increment PATCH with each build to verify latest firmware is loaded.
#ifndef SOFTWARE_VERSION_MAJOR
#  define SOFTWARE_VERSION_MAJOR 2
#endif
#ifndef SOFTWARE_VERSION_MINOR
#  define SOFTWARE_VERSION_MINOR 2
#endif
#ifndef SOFTWARE_VERSION_PATCH
#  define SOFTWARE_VERSION_PATCH 2
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
#  define ENABLE_LEVEL_SHIFTER 1  // Default to enabled for hardware protection
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
#else
#  error "HIDPICO_REVISION must be 2, 4, or 5. Current value is not recognized."
#endif

#endif // _CONFIG_H
