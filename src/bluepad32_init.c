/**
 * this file is part of amigahid-pico, (c) 2024
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluepad32 initialization
 * separated from main.c to avoid HID type conflicts between TinyUSB and btstack
 */

#if ENABLE_BLUEPAD32

#include <stdio.h>
#include <pico/cyw43_arch.h>
#include <pico/async_context.h>
#include <pico/async_context_poll.h>
#include <pico/btstack_run_loop_async_context.h>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <uni.h>
#include "bluepad32_platform.h"

// Global async context for btstack (accessed from main.c)
async_context_poll_t* g_btstack_async_context = NULL;

// Runtime state: Bluetooth enabled/disabled
static bool g_bluetooth_enabled = false;

// Initialize Bluepad32 and return the async context
async_context_poll_t* bluepad32_init(void) {
    if (g_bluetooth_enabled) {
        printf("Bluetooth already initialized\n");
        return g_btstack_async_context;
    }
    static async_context_poll_t btstack_async_context;
    
    // Initialize async_context for btstack
    if (!async_context_poll_init_with_defaults(&btstack_async_context)) {
        return NULL;
    }

    // IMPORTANT: Set the async_context BEFORE calling cyw43_arch_init()
    // This allows CYW43 to use our btstack async_context instead of creating its own
    cyw43_arch_set_async_context(&btstack_async_context.core);

    // Reset CYW43 chip before initialization (power cycle)
    // This helps if the chip is in a bad state from previous attempts
    printf("Resetting CYW43 chip (power cycle)...\n");
    #ifdef CYW43_PIN_WL_REG_ON
    gpio_init(CYW43_PIN_WL_REG_ON);
    gpio_set_dir(CYW43_PIN_WL_REG_ON, GPIO_OUT);
    gpio_put(CYW43_PIN_WL_REG_ON, 0);  // Power off
    sleep_ms(100);  // Wait 100ms
    gpio_put(CYW43_PIN_WL_REG_ON, 1);  // Power on
    sleep_ms(250);  // Wait 250ms for chip to stabilize
    #endif

    // Initialize CYW43 (WiFi/Bluetooth chip on Pico 2 W)
    printf("Initializing CYW43 (WiFi/Bluetooth chip)...\n");
    int cyw43_result = cyw43_arch_init();
    if (cyw43_result) {
        printf("ERROR: cyw43_arch_init() failed with code %d\n", cyw43_result);
        printf("Possible causes:\n");
        printf("  - Missing CYW43 firmware (CLM file)\n");
        printf("  - Hardware issue with CYW43 chip\n");
        printf("  - Power supply issue\n");
        printf("  - CYW43 chip not responding (check connections)\n");
        async_context_deinit(&btstack_async_context.core);
        return NULL;
    }
    
    printf("CYW43 initialized successfully\n");
    
    // Set custom platform before uni_init()
    uni_platform_set_custom(get_my_platform());

    // Initialize btstack run loop with async_context
    const btstack_run_loop_t* btstack_run_loop = 
        btstack_run_loop_async_context_get_instance(&btstack_async_context.core);
    btstack_run_loop_init(btstack_run_loop);

    // Initialize Bluepad32
    uni_init(0, NULL);

    g_btstack_async_context = &btstack_async_context;
    g_bluetooth_enabled = true;
    return &btstack_async_context;
}

// Deinitialize Bluepad32
void bluepad32_deinit(void) {
    if (!g_bluetooth_enabled) {
        return;  // Already disabled
    }
    
    printf("Deinitializing Bluetooth...\n");
    
    // Deinitialize CYW43
    if (g_btstack_async_context) {
        cyw43_arch_deinit();
        async_context_deinit(&g_btstack_async_context->core);
        g_btstack_async_context = NULL;
    }
    
    g_bluetooth_enabled = false;
    printf("Bluetooth deinitialized\n");
}

// Runtime control functions
void bluepad32_enable(void) {
    if (g_bluetooth_enabled) {
        printf("Bluetooth already enabled\n");
        return;
    }
    printf("Enabling Bluetooth...\n");
    bluepad32_init();
}

void bluepad32_disable(void) {
    if (!g_bluetooth_enabled) {
        printf("Bluetooth already disabled\n");
        return;
    }
    bluepad32_deinit();
}

bool bluepad32_is_enabled(void) {
    return g_bluetooth_enabled;
}

// Poll btstack async_context (non-blocking)
// IMPORTANT: This should be called regularly but not too frequently to avoid
// interfering with USB processing or keyboard timing.
void bluepad32_poll(void) {
    if (g_btstack_async_context) {
        async_context_poll(&g_btstack_async_context->core);
    }
}

#endif // ENABLE_BLUEPAD32


