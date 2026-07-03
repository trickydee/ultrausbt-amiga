/**
 * Amiga CD32 gamepad protocol — independent Port 1 and Port 2 (Rev 5).
 *
 * IRQ timing matches v2.1.2 (97a2c87) per port; dual-port state only.
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
#define CD32_PORT_COUNT 2

typedef struct {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
    uint8_t joymode;
    uint8_t clock;
    uint8_t data;
} cd32_gpio_map_t;

typedef struct {
    cd32_gpio_map_t pin;
    bool enabled;
    volatile bool joymode_high;
    volatile uint8_t shift_index;
    cd32_buttons_t buttons;
    cd32_buttons_t buttons_shadow;
} cd32_port_state_t;

static const cd32_gpio_map_t CD32_GPIO_PORT1 = {
    QM1_AMIGA_V, QM1_AMIGA_H, QM1_AMIGA_VQ, QM1_AMIGA_HQ,
    QM1_AMIGA_B3, QM1_AMIGA_B1, QM1_AMIGA_B2,
};

static const cd32_gpio_map_t CD32_GPIO_PORT2 = {
    QM2_AMIGA_V, QM2_AMIGA_H, QM2_AMIGA_VQ, QM2_AMIGA_HQ,
    QM2_AMIGA_B3, QM2_AMIGA_B1, QM2_AMIGA_B2,
};

static cd32_port_state_t g_port[CD32_PORT_COUNT];
static bool g_irq_callback_installed;

static cd32_port_state_t* cd32_state_for_port(uint8_t port) {
    if (port < 1 || port > CD32_PORT_COUNT) {
        return NULL;
    }
    return &g_port[port - 1];
}

static cd32_port_state_t* cd32_state_for_gpio(uint gpio) {
    for (int i = 0; i < CD32_PORT_COUNT; i++) {
        if (!g_port[i].enabled) {
            continue;
        }
        if (gpio == g_port[i].pin.joymode || gpio == g_port[i].pin.clock) {
            return &g_port[i];
        }
    }
    return NULL;
}

static inline void cd32_data_configure(cd32_port_state_t* ps) {
    gpio_set_function(ps->pin.data, GPIO_FUNC_SIO);
    gpio_set_pulls(ps->pin.data, false, false);
    gpio_set_dir(ps->pin.data, GPIO_OUT);
}

static inline void cd32_data_put(cd32_port_state_t* ps, bool line_low) {
    gpio_put(ps->pin.data, line_low ? 0 : 1);
}

static inline void cd32_clock_release(cd32_port_state_t* ps) {
    gpio_set_function(ps->pin.clock, GPIO_FUNC_SIO);
    gpio_set_dir(ps->pin.clock, GPIO_IN);
    gpio_set_pulls(ps->pin.clock, true, false);
    amiga_gpio_clear_cache(ps->pin.clock);
}

static inline void cd32_clock_drive_red(cd32_port_state_t* ps, bool pressed) {
    if (pressed) {
        gpio_set_function(ps->pin.clock, GPIO_FUNC_SIO);
        gpio_set_pulls(ps->pin.clock, false, false);
        gpio_set_dir(ps->pin.clock, GPIO_OUT);
        gpio_put(ps->pin.clock, 0);
    } else {
        cd32_clock_release(ps);
    }
}

static bool cd32_shift_bit_is_low(const cd32_port_state_t* ps, int index) {
    if (index < 7) {
        bool pressed = false;
        switch (index) {
            case 0: pressed = ps->buttons_shadow.blue; break;
            case 1: pressed = ps->buttons_shadow.red; break;
            case 2: pressed = ps->buttons_shadow.yellow; break;
            case 3: pressed = ps->buttons_shadow.green; break;
            case 4: pressed = ps->buttons_shadow.ff; break;
            case 5: pressed = ps->buttons_shadow.rew; break;
            case 6: pressed = ps->buttons_shadow.pause; break;
            default: break;
        }
        return pressed;
    }
    if (index == 7) {
        return false;
    }
    return true;
}

static void cd32_present_shift_bit(cd32_port_state_t* ps, int index) {
    cd32_data_put(ps, cd32_shift_bit_is_low(ps, index));
}

static void cd32_update_dumb_outputs(cd32_port_state_t* ps) {
    if (!ps->enabled || !ps->joymode_high) {
        return;
    }
    cd32_clock_drive_red(ps, ps->buttons_shadow.red);
    cd32_data_put(ps, ps->buttons_shadow.blue);
}

static void cd32_configure_clock_for_joymode(cd32_port_state_t* ps) {
    if (ps->joymode_high) {
        cd32_update_dumb_outputs(ps);
    } else {
        cd32_clock_release(ps);
    }
}

static void cd32_on_latch_falling(cd32_port_state_t* ps) {
    uint32_t save = save_and_disable_interrupts();
    ps->buttons_shadow = ps->buttons;
    ps->shift_index = 0;
    cd32_present_shift_bit(ps, 0);
    restore_interrupts(save);
}

static void cd32_on_clock_falling(cd32_port_state_t* ps) {
    if (ps->joymode_high) {
        return;
    }
    /* Amiga samples DATA on CLOCK rise; prepare the next bit after each fall so
     * the line is stable before the following rise (reduces adjacent-button ghosts). */
    uint32_t save = save_and_disable_interrupts();
    if (ps->shift_index < (CD32_SHIFT_BITS - 1)) {
        ps->shift_index++;
        cd32_present_shift_bit(ps, ps->shift_index);
    }
    restore_interrupts(save);
}

static void cd32_gpio_irq(uint gpio, uint32_t events) {
    gpio_acknowledge_irq(gpio, events);

    cd32_port_state_t* ps = cd32_state_for_gpio(gpio);
    if (ps == NULL) {
        return;
    }

    if (gpio == ps->pin.joymode) {
        ps->joymode_high = gpio_get(ps->pin.joymode);
        cd32_configure_clock_for_joymode(ps);
        if ((events & GPIO_IRQ_EDGE_FALL) && !ps->joymode_high) {
            cd32_on_latch_falling(ps);
        }
        return;
    }

    if (gpio == ps->pin.clock && !ps->joymode_high) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            cd32_on_clock_falling(ps);
        }
    }
}

static void cd32_teardown_port_gpios(cd32_port_state_t* ps) {
    gpio_set_irq_enabled(ps->pin.joymode, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_set_irq_enabled(ps->pin.clock, GPIO_IRQ_EDGE_FALL, false);
}

static void cd32_setup_port_gpios(cd32_port_state_t* ps) {
    amiga_gpio_init_active_low(ps->pin.up, false);
    amiga_gpio_init_active_low(ps->pin.down, false);
    amiga_gpio_init_active_low(ps->pin.left, false);
    amiga_gpio_init_active_low(ps->pin.right, false);

    gpio_set_function(ps->pin.joymode, GPIO_FUNC_SIO);
    gpio_set_dir(ps->pin.joymode, GPIO_IN);
    gpio_set_pulls(ps->pin.joymode, true, false);

    cd32_data_configure(ps);
    cd32_data_put(ps, false);
    cd32_clock_release(ps);

    ps->joymode_high = gpio_get(ps->pin.joymode);
    ps->shift_index = 0;
    memset(&ps->buttons, 0, sizeof(ps->buttons));
    ps->buttons_shadow = ps->buttons;

    if (!g_irq_callback_installed) {
        gpio_set_irq_callback(&cd32_gpio_irq);
        g_irq_callback_installed = true;
    }

    gpio_set_irq_enabled(ps->pin.joymode, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(ps->pin.clock, GPIO_IRQ_EDGE_FALL, true);

    cd32_configure_clock_for_joymode(ps);
}

static void cd32_apply_dpad(cd32_port_state_t* ps, uint8_t direction_bits) {
    amiga_gpio_set_active_low(ps->pin.up, (direction_bits & 0x01) != 0);
    amiga_gpio_set_active_low(ps->pin.down, (direction_bits & 0x02) != 0);
    amiga_gpio_set_active_low(ps->pin.left, (direction_bits & 0x04) != 0);
    amiga_gpio_set_active_low(ps->pin.right, (direction_bits & 0x08) != 0);
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
    cd32_port_state_t* ps = cd32_state_for_port(port);
    if (ps == NULL) {
        return;
    }

    if (enabled == ps->enabled) {
        return;
    }

    if (!enabled) {
        ps->enabled = false;
        cd32_teardown_port_gpios(ps);
        cd32_restore_port_gpios(port);
        printf("[CD32] Port %u standard joystick mode\n", port);
        return;
    }

    ps->pin = (port == 1) ? CD32_GPIO_PORT1 : CD32_GPIO_PORT2;
    ps->enabled = true;
    cd32_setup_port_gpios(ps);
    printf("[CD32] Port %u CD32 mode enabled\n", port);
}

static void cd32_update(cd32_port_state_t* ps, const cd32_buttons_t* buttons, uint8_t direction_bits) {
    if (!ps->enabled || buttons == NULL) {
        return;
    }

    uint32_t save = save_and_disable_interrupts();
    ps->buttons = *buttons;
    restore_interrupts(save);

    cd32_apply_dpad(ps, direction_bits);

    if (ps->joymode_high) {
        ps->buttons_shadow = ps->buttons;
        cd32_update_dumb_outputs(ps);
    }
}

static void cd32_update_dpad(cd32_port_state_t* ps, uint8_t direction_bits) {
    if (!ps->enabled) {
        return;
    }
    cd32_apply_dpad(ps, direction_bits);
}

static void cd32_legacy_button(cd32_port_state_t* ps, uint8_t which, bool pressed) {
    if (!ps->enabled) {
        return;
    }

    uint32_t save = save_and_disable_interrupts();
    switch (which) {
        case 0: ps->buttons.red = pressed; break;
        case 1: ps->buttons.blue = pressed; break;
        case 2: ps->buttons.yellow = pressed; break;
        default: break;
    }
    restore_interrupts(save);

    if (ps->joymode_high) {
        ps->buttons_shadow = ps->buttons;
        cd32_update_dumb_outputs(ps);
    }
}

void cd32_service(void) {
}

void cd32_port1_init(void) {
    memset(g_port, 0, sizeof(g_port));
    g_irq_callback_installed = false;
}

void cd32_port2_init(void) {}

bool cd32_port1_is_enabled(void) {
    return g_port[0].enabled;
}

bool cd32_port2_is_enabled(void) {
    return g_port[1].enabled;
}

void cd32_port1_set_enabled(bool enabled) { cd32_set_port_enabled(1, enabled); }
void cd32_port2_set_enabled(bool enabled) { cd32_set_port_enabled(2, enabled); }

void cd32_port1_toggle(void) { cd32_port1_set_enabled(!cd32_port1_is_enabled()); }
void cd32_port2_toggle(void) { cd32_port2_set_enabled(!cd32_port2_is_enabled()); }

void cd32_port1_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    cd32_update(&g_port[0], buttons, direction_bits);
}

void cd32_port2_update(const cd32_buttons_t* buttons, uint8_t direction_bits) {
    cd32_update(&g_port[1], buttons, direction_bits);
}

void cd32_port1_update_dpad(uint8_t direction_bits) {
    cd32_update_dpad(&g_port[0], direction_bits);
}

void cd32_port2_update_dpad(uint8_t direction_bits) {
    cd32_update_dpad(&g_port[1], direction_bits);
}

void cd32_port1_legacy_button(enum amiga_joystick_port1_buttons button, bool pressed) {
    uint8_t which = 255;
    switch (button) {
        case AJ1_FIRE: which = 0; break;
        case AJ1_BUTTON2: which = 1; break;
        case AJ1_BUTTON3: which = 2; break;
        default: break;
    }
    if (which != 255) {
        cd32_legacy_button(&g_port[0], which, pressed);
    }
}

void cd32_port2_legacy_button(enum amiga_joystick_port2_buttons button, bool pressed) {
    uint8_t which = 255;
    switch (button) {
        case AJ2_FIRE: which = 0; break;
        case AJ2_BUTTON2: which = 1; break;
        case AJ2_BUTTON3: which = 2; break;
        default: break;
    }
    if (which != 255) {
        cd32_legacy_button(&g_port[1], which, pressed);
    }
}

#else

void cd32_port1_init(void) {}
void cd32_port2_init(void) {}
void cd32_service(void) {}
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
