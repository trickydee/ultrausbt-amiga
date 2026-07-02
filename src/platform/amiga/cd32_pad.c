/**
 * Amiga CD32 gamepad protocol — Port 1 and Port 2 (Rev 5).
 */

#include "cd32_pad.h"
#include "config.h"
#include "joystick_port1.h"
#include "joystick_port2.h"
#include "platform/common/gpio_util.h"

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <stdio.h>
#include <string.h>

#if HIDPICO_REVISION == 5

#define CD32_SHIFT_BITS 9

typedef struct {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t joymode;
    uint8_t clock;
    uint8_t data;
} cd32_gpio_map_t;

static const cd32_gpio_map_t CD32_GPIO_PORT1 = {
    QM1_AMIGA_V, QM1_AMIGA_H, QM1_AMIGA_VQ, QM1_AMIGA_HQ,
    QM1_AMIGA_B3, QM1_AMIGA_B1, QM1_AMIGA_B2,
};

static const cd32_gpio_map_t CD32_GPIO_PORT2 = {
    QM2_AMIGA_V, QM2_AMIGA_H, QM2_AMIGA_VQ, QM2_AMIGA_HQ,
    QM2_AMIGA_B3, QM2_AMIGA_B1, QM2_AMIGA_B2,
};

static cd32_gpio_map_t g_pin;
static uint8_t g_active_port;
static bool g_cd32_enabled;
static volatile bool g_joymode_high = true;
static volatile uint8_t g_shift_index;

static cd32_buttons_t g_buttons;
static cd32_buttons_t g_buttons_shadow;

static void cd32_update_dumb_outputs(void);

static inline void cd32_data_out(bool line_low) {
    gpio_set_function(g_pin.data, GPIO_FUNC_SIO);
    gpio_set_pulls(g_pin.data, false, false);
    gpio_set_dir(g_pin.data, GPIO_OUT);
    gpio_put(g_pin.data, line_low ? 0 : 1);
}

static inline void cd32_clock_release(void) {
    gpio_set_function(g_pin.clock, GPIO_FUNC_SIO);
    gpio_set_dir(g_pin.clock, GPIO_IN);
    gpio_set_pulls(g_pin.clock, true, false);
    amiga_gpio_clear_cache(g_pin.clock);
}

static inline void cd32_clock_drive_red(bool pressed) {
    if (pressed) {
        gpio_set_function(g_pin.clock, GPIO_FUNC_SIO);
        gpio_set_pulls(g_pin.clock, false, false);
        gpio_set_dir(g_pin.clock, GPIO_OUT);
        gpio_put(g_pin.clock, 0);
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
    if (gpio == g_pin.joymode) {
        g_joymode_high = gpio_get(g_pin.joymode);
        cd32_configure_clock_for_joymode();
        if ((events & GPIO_IRQ_EDGE_FALL) && !g_joymode_high) {
            cd32_on_latch_falling();
        }
        return;
    }
    if (gpio == g_pin.clock && (events & GPIO_IRQ_EDGE_RISE) && !g_joymode_high) {
        cd32_on_clock_rising();
    }
}

static void cd32_teardown_gpios(void) {
    gpio_set_irq_enabled(g_pin.joymode, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_enabled(g_pin.clock, GPIO_IRQ_EDGE_RISE, false);
}

static void cd32_setup_gpios(void) {
    amiga_gpio_init_active_low(g_pin.up, false);
    amiga_gpio_init_active_low(g_pin.down, false);
    amiga_gpio_init_active_low(g_pin.left, false);
    amiga_gpio_init_active_low(g_pin.right, false);

    gpio_set_function(g_pin.joymode, GPIO_FUNC_SIO);
    gpio_set_dir(g_pin.joymode, GPIO_IN);
    gpio_set_pulls(g_pin.joymode, true, false);

    gpio_set_function(g_pin.clock, GPIO_FUNC_SIO);
    cd32_clock_release();

    cd32_data_out(false);

    g_joymode_high = gpio_get(g_pin.joymode);
    g_shift_index = 0;
    memset(&g_buttons, 0, sizeof(g_buttons));
    g_buttons_shadow = g_buttons;

    gpio_set_irq_callback(&cd32_gpio_irq);
    gpio_set_irq_enabled(g_pin.joymode, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(g_pin.clock, GPIO_IRQ_EDGE_RISE, true);

    cd32_configure_clock_for_joymode();
}

static void cd32_apply_dpad(uint8_t direction_bits) {
    amiga_gpio_set_active_low(g_pin.up, (direction_bits & 0x01) != 0);
    amiga_gpio_set_active_low(g_pin.down, (direction_bits & 0x02) != 0);
    amiga_gpio_set_active_low(g_pin.left, (direction_bits & 0x04) != 0);
    amiga_gpio_set_active_low(g_pin.right, (direction_bits & 0x08) != 0);
}

static void cd32_restore_port_gpios(uint8_t port) {
    if (port == 1) {
        amiga_joystick_port1_init();
    } else if (port == 2) {
        amiga_joystick_port2_reset();
        amiga_joystick_port2_init();
    }
}

static void cd32_set_port_enabled(uint8_t port, bool enabled) {
    if (enabled && g_cd32_enabled && g_active_port == port) {
        return;
    }
    if (!enabled && (!g_cd32_enabled || g_active_port != port)) {
        return;
    }

    if (g_cd32_enabled) {
        g_cd32_enabled = false;
        cd32_teardown_gpios();
        cd32_restore_port_gpios(g_active_port);
        g_active_port = 0;
    }

    if (!enabled) {
        printf("[CD32] Port %u standard joystick mode\n", port);
        return;
    }

    if (port == 1) {
        g_pin = CD32_GPIO_PORT1;
    } else {
        g_pin = CD32_GPIO_PORT2;
    }

    cd32_setup_gpios();
    g_active_port = port;
    g_cd32_enabled = true;
    printf("[CD32] Port %u CD32 mode enabled\n", port);
}

static void cd32_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
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

static void cd32_update_dpad(uint8_t direction_bits) {
    if (!g_cd32_enabled) {
        return;
    }
    cd32_apply_dpad(direction_bits);
}

void cd32_port1_init(void) { g_active_port = 0; g_cd32_enabled = false; }
void cd32_port2_init(void) { g_active_port = 0; g_cd32_enabled = false; }

bool cd32_port1_is_enabled(void) { return g_cd32_enabled && g_active_port == 1; }
bool cd32_port2_is_enabled(void) { return g_cd32_enabled && g_active_port == 2; }

void cd32_port1_set_enabled(bool enabled) { cd32_set_port_enabled(1, enabled); }
void cd32_port2_set_enabled(bool enabled) { cd32_set_port_enabled(2, enabled); }

void cd32_port1_toggle(void) { cd32_port1_set_enabled(!cd32_port1_is_enabled()); }
void cd32_port2_toggle(void) { cd32_port2_set_enabled(!cd32_port2_is_enabled()); }

void cd32_port1_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    if (!cd32_port1_is_enabled()) return;
    cd32_update(buttons, direction_bits);
}

void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    if (!cd32_port2_is_enabled()) return;
    cd32_update(buttons, direction_bits);
}

void cd32_port1_update_dpad(uint8_t direction_bits) {
    if (!cd32_port1_is_enabled()) return;
    cd32_update_dpad(direction_bits);
}

void cd32_port2_update_dpad(uint8_t direction_bits) {
    if (!cd32_port2_is_enabled()) return;
    cd32_update_dpad(direction_bits);
}

void cd32_port1_legacy_button(enum amiga_joystick_port1_buttons button, bool pressed) {
    if (!cd32_port1_is_enabled()) return;
    uint32_t save = save_and_disable_interrupts();
    switch (button) {
        case AJ1_FIRE:    g_buttons.red = pressed; break;
        case AJ1_BUTTON2: g_buttons.blue = pressed; break;
        case AJ1_BUTTON3: g_buttons.yellow = pressed; break;
        default: break;
    }
    restore_interrupts(save);
    if (g_joymode_high) {
        g_buttons_shadow = g_buttons;
        cd32_update_dumb_outputs();
    }
}

void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed) {
    if (!cd32_port2_is_enabled()) return;
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

void cd32_port1_init(void) {}
void cd32_port2_init(void) {}
bool cd32_port1_is_enabled(void) { return false; }
bool cd32_port2_is_enabled(void) { return false; }
void cd32_port1_set_enabled(bool enabled) { (void)enabled; }
void cd32_port2_set_enabled(bool enabled) { (void)enabled; }
void cd32_port1_toggle(void) {}
void cd32_port2_toggle(void) {}
void cd32_port1_update(const cd32_buttons_t* b, uint8_t d) { (void)b; (void)d; }
void cd32_port2_update(const cd32_buttons_t* b, uint8_t d) { (void)b; (void)d; }
void cd32_port1_update_dpad(uint8_t d) { (void)d; }
void cd32_port2_update_dpad(uint8_t d) { (void)d; }
void cd32_port1_legacy_button(enum amiga_joystick_port1_buttons b, bool p) { (void)b; (void)p; }
void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons b, bool p) { (void)b; (void)p; }

#endif
