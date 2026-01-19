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

#ifndef HIDPICO_REVISION
#  define HIDPICO_REVISION 4
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
#  define I2C_PORT      i2c1
#  define I2C_PIN_SDA   2
#  define I2C_PIN_SCL   3
#  define I2C_IRQN      24

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

// Joystick Port 2 (Revision 5 - verified hardware pinout)
// Each direction uses a separate GPIO pin (not shared like mouse quadrature)
// GPIO 20 (Pin 6) = Button 1 (Fire)
// GPIO 21 (Pin 4) = RIGHT direction
// GPIO 22 (Pin 3) = LEFT direction
// GPIO 26 (Pin 2) = DOWN direction
// GPIO 27 (Pin 1) = UP direction
#  define QM2_AMIGA_B1  20  // Button 1 (Fire) - Pin 6
#  define QM2_AMIGA_B2  19  // Button 2
#  define QM2_AMIGA_B3  18  // Button 3
#  define QM2_AMIGA_HQ  21  // RIGHT direction (Pin 4) - reused HQ pin assignment
#  define QM2_AMIGA_VQ  22  // LEFT direction (Pin 3) - reused VQ pin assignment
#  define QM2_AMIGA_H   26  // DOWN direction (Pin 2) - GPIO26_ADC0, physical pin 31
#  define QM2_AMIGA_V   27  // UP direction (Pin 1) - GPIO27_ADC1, physical pin 32
#else
#  error "HIDPICO_REVISION must be 2, 4, or 5. Current value is not recognized."
#endif

#endif // _CONFIG_H
