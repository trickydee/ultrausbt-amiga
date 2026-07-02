/**
 * Amiga CD32 gamepad protocol — port 2 implementation (Rev 5).
 */

#include "cd32_pad.h"
#include "config.h"
#include "joystick_port2.h"
#include "platform/common/gpio_util.h"

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <stdio.h>
#include <string.h>

#if HIDPICO_REVISION == 5

#define CD32_PIN_UP     QM2_AMIGA_V
#define CD32_PIN_DOWN   QM2_AMIGA_H
#define CD32_PIN_LEFT   QM2_AMIGA_VQ
#define CD32_PIN_RIGHT  QM2_AMIGA_HQ
#define CD32_PIN_JOYMODE QM2_AMIGA_B3
#define CD32_PIN_CLOCK  QM2_AMIGA_B1
#define CD32_PIN_DATA   QM2_AMIGA_B2

#define CD32_SHIFT_BITS 9

static bool g_cd32_enabled;
static volatile bool g_joymode_high = true;
static volatile uint8_t g_shift_index;

static cd32_buttons_t g_buttons;
static cd32_buttons_t g_buttons_shadow;

static void cd32_update_dumb_outputs(void);

/** DATA (pin 9) — always driven as OUTPUT; ISR-safe (KTRL-CD32 style). */
static inline void cd32_data_out(bool line_low) {
    gpio_set_function(CD32_PIN_DATA, GPIO_FUNC_SIO);
    gpio_set_pulls(CD32_PIN_DATA, false, false);
    gpio_set_dir(CD32_PIN_DATA, GPIO_OUT);
    gpio_put(CD32_PIN_DATA, line_low ? 0 : 1);
}

/** CLOCK (pin 6) — input in shift mode; open-drain style in dumb mode. */
static inline void cd32_clock_release(void) {
    gpio_set_function(CD32_PIN_CLOCK, GPIO_FUNC_SIO);
    gpio_set_dir(CD32_PIN_CLOCK, GPIO_IN);
    gpio_set_pulls(CD32_PIN_CLOCK, true, false);
    amiga_gpio_clear_cache(CD32_PIN_CLOCK);
}

static inline void cd32_clock_drive_red(bool pressed) {
    if (pressed) {
        gpio_set_function(CD32_PIN_CLOCK, GPIO_FUNC_SIO);
        gpio_set_pulls(CD32_PIN_CLOCK, false, false);
        gpio_set_dir(CD32_PIN_CLOCK, GPIO_OUT);
        gpio_put(CD32_PIN_CLOCK, 0);
    } else {
        cd32_clock_release();
    }
}

static void cd32_drive_data_line(bool line_low) {
    cd32_data_out(line_low);
}

static bool cd32_shift_bit_is_low(int index) {
    if (index < 7) {
        bool pressed = false;
        switch (index) {
            case 0: pressed = g_buttons_shadow.blue; break;
            case 1: pressed = g_buttons_shadow.red; break;
            case 2: pressed = g_buttons_shadow.yellow; break;
            case 3: pressed = g_buttons_shadow.green; break;
            case 4: pressed = g_buttons_shadow.ff; break;
            case 5: pressed = g_buttons_shadow.rew; break;
            case 6: pressed = g_buttons_shadow.pause; break;
            default: break;
        }
        return pressed;
    }
    if (index == 7) {
        return false;
    }
    return true;
}

static void cd32_present_shift_bit(int index) {
    cd32_drive_data_line(cd32_shift_bit_is_low(index));
}

static void cd32_configure_clock_for_joymode(void) {
    if (g_joymode_high) {
        cd32_update_dumb_outputs();
    } else {
        cd32_clock_release();
    }
}

static void cd32_update_dumb_outputs(void) {
    if (!g_cd32_enabled || !g_joymode_high) {
        return;
    }
    cd32_clock_drive_red(g_buttons_shadow.red);
    cd32_data_out(g_buttons_shadow.blue);
}

static void cd32_on_latch_falling(void) {
    uint32_t save = save_and_disable_interrupts();
    g_buttons_shadow = g_buttons;
    g_shift_index = 0;
    cd32_present_shift_bit(0);
    restore_interrupts(save);
}

static void cd32_on_clock_rising(void) {
    if (g_joymode_high) {
        return;
    }
    uint32_t save = save_and_disable_interrupts();
    if (g_shift_index < (CD32_SHIFT_BITS - 1)) {
        g_shift_index++;
    }
    cd32_present_shift_bit(g_shift_index);
    restore_interrupts(save);
}

static void cd32_gpio_irq(uint gpio, uint32_t events) {
    if (gpio == CD32_PIN_JOYMODE) {
        g_joymode_high = gpio_get(CD32_PIN_JOYMODE);
        /* Release CLOCK before latch so we never fight the Amiga in shift mode. */
        cd32_configure_clock_for_joymode();
        if ((events & GPIO_IRQ_EDGE_FALL) && !g_joymode_high) {
            cd32_on_latch_falling();
        }
        return;
    }
    if (gpio == CD32_PIN_CLOCK && (events & GPIO_IRQ_EDGE_RISE) && !g_joymode_high) {
        cd32_on_clock_rising();
    }
}

static void cd32_setup_gpios(void) {
    amiga_gpio_init_active_low(CD32_PIN_UP, false);
    amiga_gpio_init_active_low(CD32_PIN_DOWN, false);
    amiga_gpio_init_active_low(CD32_PIN_LEFT, false);
    amiga_gpio_init_active_low(CD32_PIN_RIGHT, false);

    gpio_set_function(CD32_PIN_JOYMODE, GPIO_FUNC_SIO);
    gpio_set_dir(CD32_PIN_JOYMODE, GPIO_IN);
    gpio_set_pulls(CD32_PIN_JOYMODE, true, false);

    gpio_set_function(CD32_PIN_CLOCK, GPIO_FUNC_SIO);
    cd32_clock_release();

    cd32_data_out(false);

    g_joymode_high = gpio_get(CD32_PIN_JOYMODE);
    g_shift_index = 0;
    memset(&g_buttons, 0, sizeof(g_buttons));
    g_buttons_shadow = g_buttons;

    gpio_set_irq_callback(&cd32_gpio_irq);
    gpio_set_irq_enabled(CD32_PIN_JOYMODE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(CD32_PIN_CLOCK, GPIO_IRQ_EDGE_RISE, true);

    cd32_configure_clock_for_joymode();
}

static void cd32_teardown_gpios(void) {
    gpio_set_irq_enabled(CD32_PIN_JOYMODE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_enabled(CD32_PIN_CLOCK, GPIO_IRQ_EDGE_RISE, false);
}

static void cd32_apply_dpad(uint8_t direction_bits) {
    amiga_gpio_set_active_low(CD32_PIN_UP, (direction_bits & 0x01) != 0);
    amiga_gpio_set_active_low(CD32_PIN_DOWN, (direction_bits & 0x02) != 0);
    amiga_gpio_set_active_low(CD32_PIN_LEFT, (direction_bits & 0x04) != 0);
    amiga_gpio_set_active_low(CD32_PIN_RIGHT, (direction_bits & 0x08) != 0);
}

void cd32_port2_init(void) {
    g_cd32_enabled = false;
}

bool cd32_port2_is_enabled(void) {
    return g_cd32_enabled;
}

void cd32_port2_set_enabled(bool enabled) {
    if (enabled == g_cd32_enabled) {
        return;
    }
    if (enabled) {
        cd32_setup_gpios();
        g_cd32_enabled = true;
        printf("[CD32] Port 2 CD32 mode enabled\n");
    } else {
        g_cd32_enabled = false;
        cd32_teardown_gpios();
        amiga_joystick_port2_reset();
        amiga_joystick_port2_init();
        printf("[CD32] Port 2 standard joystick mode\n");
    }
}

void cd32_port2_toggle(void) {
    cd32_port2_set_enabled(!g_cd32_enabled);
}

void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    if (!g_cd32_enabled || buttons == NULL) {
        return;
    }
    uint32_t save = save_and_disable_interrupts();
    g_buttons = *buttons;
    restore_interrupts(save);

    cd32_apply_dpad(direction_bits);

    if (g_joymode_high) {
        g_buttons_shadow = g_buttons;
        cd32_update_dumb_outputs();
    }
}

void cd32_port2_update_dpad(uint8_t direction_bits) {
    if (!g_cd32_enabled) {
        return;
    }
    cd32_apply_dpad(direction_bits);
}

void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed) {
    if (!g_cd32_enabled) {
        return;
    }
    uint32_t save = save_and_disable_interrupts();
    switch (button) {
        case AJ2_FIRE:    g_buttons.red = pressed; break;
        case AJ2_BUTTON2: g_buttons.blue = pressed; break;
        case AJ2_BUTTON3: g_buttons.yellow = pressed; break;
        default: break;
    }
    restore_interrupts(save);
    if (g_joymode_high) {
        g_buttons_shadow = g_buttons;
        cd32_update_dumb_outputs();
    }
}

#else

void cd32_port2_init(void) {}
bool cd32_port2_is_enabled(void) { return false; }
void cd32_port2_set_enabled(bool enabled) { (void)enabled; }
void cd32_port2_toggle(void) {}
void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    (void)buttons;
    (void)direction_bits;
}
void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed) {
    (void)button;
    (void)pressed;
}
void cd32_port2_update_dpad(uint8_t direction_bits) { (void)direction_bits; }

#endif
