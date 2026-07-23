/**
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 *
 * Released under the Eclipse Public License 2.0 (EPL-2.0).
 * https://spdx.org/licenses/EPL-2.0
 *
 * Part of ultrausbt-amiga (fork of amigahid-pico by just nine / borb).
 *
 * Mouse and port configuration persistence using flash storage.
 */

#include "mouse_config.h"
#include "config.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>

#ifndef PICO_FLASH_BANK_TOTAL_SIZE
#define PICO_FLASH_BANK_TOTAL_SIZE (FLASH_SECTOR_SIZE * 2u)
#endif

#define CONFIG_FLASH_OFFSET_LEGACY (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

static uint32_t port_config_flash_offset(void)
{
#if PICO_RP2350 && PICO_RP2350_A2_SUPPORTED
    const uint32_t bt_bank =
        PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE - PICO_FLASH_BANK_TOTAL_SIZE;
#else
    const uint32_t bt_bank = PICO_FLASH_SIZE_BYTES - PICO_FLASH_BANK_TOTAL_SIZE;
#endif
    return bt_bank - FLASH_SECTOR_SIZE;
}

#define CONFIG_MAGIC_LEGACY 0x4D4F5553  /* "MOUS" */
#define CONFIG_MAGIC        0x414D4947  /* "AMIG" */
#define CONFIG_VERSION      2

typedef struct {
    uint32_t magic;
    uint32_t version;
    mouse_type_t mouse_type;
    uint8_t port1_mode;
    uint8_t port2_cd32;
    uint8_t usb_device_mode;  // reuses a former pad byte; 0xFF on pre-v2 configs
    uint8_t pad[1];
    uint32_t reserved[12];
} port_config_flash_t;

static uint8_t __attribute__((aligned(256))) flash_buffer[FLASH_SECTOR_SIZE];
static uint32_t g_config_flash_offset;
static bool g_port_config_pending;
static port_config_data_t g_port_config_pending_data;

static bool port1_mode_valid(uint8_t mode) {
    return mode <= (uint8_t)PORT1_MODE_MOUSE_ATARI;
}

static void port_config_flash_write(void* param) {
    const port_config_data_t* src = (const port_config_data_t*)param;
    port_config_flash_t* config = (port_config_flash_t*)flash_buffer;

    memset(flash_buffer, 0xFF, FLASH_SECTOR_SIZE);
    config->magic = CONFIG_MAGIC;
    config->version = CONFIG_VERSION;
    config->mouse_type = src->mouse_type;
    config->port1_mode = (uint8_t)src->port1_mode;
    config->port2_cd32 = src->port2_cd32 ? 1 : 0;
    config->usb_device_mode = src->usb_device_mode ? 1 : 0;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(g_config_flash_offset, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    ints = save_and_disable_interrupts();
    flash_range_program(g_config_flash_offset, flash_buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

static bool port_config_save_now(const port_config_data_t* config) {
    if (config == NULL) {
        return false;
    }

#ifdef DEBUG_MESSAGES
    printf("[CONFIG] Saving: mouse=%s port1=%u port2_cd32=%d\n",
           config->mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga",
           (unsigned)config->port1_mode,
           config->port2_cd32 ? 1 : 0);
#endif

    // flash_safe_execute() coordinates with core1's flash lockout victim, which is
    // only installed when core1 is running (host mode launches it via the quad-mouse
    // core). In USB device mode core1 is never started, so the lockout victim is not
    // initialised and flash_safe_execute() would fail. In that single-core context it
    // is safe to write directly (interrupts are disabled inside the write helper).
    if (multicore_lockout_victim_is_initialized(1)) {
        int result = flash_safe_execute(port_config_flash_write, (void*)config, 5000);
        if (result != 0) {
            printf("[CONFIG] ERROR: flash save failed (%d)\n", result);
            return false;
        }
    } else {
#ifdef DEBUG_MESSAGES
        printf("[CONFIG] core1 lockout not initialised; writing flash directly\n");
#endif
        port_config_flash_write((void*)config);
    }
    return true;
}

void port_config_load(port_config_data_t* out) {
    if (out == NULL) {
        return;
    }

    g_config_flash_offset = port_config_flash_offset();

    out->mouse_type = MOUSE_TYPE_AMIGA;
    out->port1_mode = PORT1_MODE_MOUSE;
    out->port2_cd32 = false;
    out->usb_device_mode = 0;

    const port_config_flash_t* flash =
        (const port_config_flash_t*)(XIP_BASE + g_config_flash_offset);

    if (flash->magic == CONFIG_MAGIC && flash->version >= CONFIG_VERSION) {
        if (flash->mouse_type == MOUSE_TYPE_AMIGA || flash->mouse_type == MOUSE_TYPE_ATARI) {
            out->mouse_type = flash->mouse_type;
        }
        if (port1_mode_valid(flash->port1_mode)) {
            out->port1_mode = (port1_mode_t)flash->port1_mode;
        }
        out->port2_cd32 = flash->port2_cd32 != 0;
        out->usb_device_mode = (flash->usb_device_mode == 1) ? 1 : 0;
        printf("[CONFIG] Loaded v%lu: mouse=%s port1=%u port2_cd32=%d\n",
               (unsigned long)flash->version,
               out->mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga",
               (unsigned)out->port1_mode,
               out->port2_cd32 ? 1 : 0);
        return;
    }

    const port_config_flash_t* legacy_sector =
        (const port_config_flash_t*)(XIP_BASE + CONFIG_FLASH_OFFSET_LEGACY);
    if (legacy_sector->magic == CONFIG_MAGIC && legacy_sector->version >= CONFIG_VERSION) {
        if (legacy_sector->mouse_type == MOUSE_TYPE_AMIGA || legacy_sector->mouse_type == MOUSE_TYPE_ATARI) {
            out->mouse_type = legacy_sector->mouse_type;
        }
        if (port1_mode_valid(legacy_sector->port1_mode)) {
            out->port1_mode = (port1_mode_t)legacy_sector->port1_mode;
        }
        out->port2_cd32 = legacy_sector->port2_cd32 != 0;
        printf("[CONFIG] Migrating config from legacy flash sector\n");
        port_config_save_now(out);
        return;
    }

    if (legacy_sector->magic == CONFIG_MAGIC_LEGACY) {
        typedef struct {
            uint32_t magic;
            mouse_type_t mouse_type;
        } port_config_legacy_t;
        const port_config_legacy_t* legacy_cfg = (const port_config_legacy_t*)legacy_sector;
        if (legacy_cfg->mouse_type == MOUSE_TYPE_AMIGA || legacy_cfg->mouse_type == MOUSE_TYPE_ATARI) {
            out->mouse_type = legacy_cfg->mouse_type;
        }
        printf("[CONFIG] Migrated legacy MOUS config (mouse type only)\n");
        return;
    }

    printf("[CONFIG] No valid config found, using defaults\n");
}

bool port_config_save(const port_config_data_t* config) {
    if (config == NULL) {
        return false;
    }

    if (core1_get_bt_pause_depth() > 0) {
        g_port_config_pending_data = *config;
        g_port_config_pending = true;
#ifdef DEBUG_MESSAGES
        printf("[CONFIG] Deferred save during BT enumeration\n");
#endif
        return true;
    }

    g_port_config_pending = false;
    return port_config_save_now(config);
}

void port_config_flush_pending(void) {
    if (!g_port_config_pending || core1_get_bt_pause_depth() > 0) {
        return;
    }
    g_port_config_pending = false;
    port_config_save_now(&g_port_config_pending_data);
}

mouse_type_t mouse_config_load(void) {
    port_config_data_t config;
    port_config_load(&config);
    return config.mouse_type;
}

bool mouse_config_save(mouse_type_t mouse_type) {
    port_config_data_t config;
    port_config_load(&config);
    config.mouse_type = mouse_type;
    return port_config_save(&config);
}
