/**
 * Mouse and port configuration persistence using flash storage.
 */

#include "mouse_config.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>

#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_MAGIC_LEGACY 0x4D4F5553  /* "MOUS" */
#define CONFIG_MAGIC        0x414D4947  /* "AMIG" */
#define CONFIG_VERSION      2

typedef struct {
    uint32_t magic;
    uint32_t version;
    mouse_type_t mouse_type;
    uint8_t port1_mode;
    uint8_t port2_cd32;
    uint8_t pad[2];
    uint32_t reserved[12];
} port_config_flash_t;

static uint8_t __attribute__((aligned(256))) flash_buffer[FLASH_SECTOR_SIZE];

static bool port1_mode_valid(uint8_t mode) {
    return mode <= (uint8_t)PORT1_MODE_CD32;
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

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    ints = save_and_disable_interrupts();
    flash_range_program(CONFIG_FLASH_OFFSET, flash_buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

void port_config_load(port_config_data_t* out) {
    if (out == NULL) {
        return;
    }

    out->mouse_type = MOUSE_TYPE_AMIGA;
    out->port1_mode = PORT1_MODE_MOUSE;
    out->port2_cd32 = false;

    const port_config_flash_t* flash = (const port_config_flash_t*)(XIP_BASE + CONFIG_FLASH_OFFSET);

    if (flash->magic == CONFIG_MAGIC && flash->version >= CONFIG_VERSION) {
        if (flash->mouse_type == MOUSE_TYPE_AMIGA || flash->mouse_type == MOUSE_TYPE_ATARI) {
            out->mouse_type = flash->mouse_type;
        }
        if (port1_mode_valid(flash->port1_mode)) {
            out->port1_mode = (port1_mode_t)flash->port1_mode;
        }
        out->port2_cd32 = flash->port2_cd32 != 0;
        printf("[CONFIG] Loaded v%d: mouse=%s port1=%u port2_cd32=%d\n",
               flash->version,
               out->mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga",
               (unsigned)out->port1_mode,
               out->port2_cd32 ? 1 : 0);
        return;
    }

    if (flash->magic == CONFIG_MAGIC_LEGACY) {
        typedef struct {
            uint32_t magic;
            mouse_type_t mouse_type;
        } port_config_legacy_t;
        const port_config_legacy_t* legacy = (const port_config_legacy_t*)flash;
        if (legacy->mouse_type == MOUSE_TYPE_AMIGA || legacy->mouse_type == MOUSE_TYPE_ATARI) {
            out->mouse_type = legacy->mouse_type;
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

    printf("[CONFIG] Saving: mouse=%s port1=%u port2_cd32=%d\n",
           config->mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga",
           (unsigned)config->port1_mode,
           config->port2_cd32 ? 1 : 0);

    int result = flash_safe_execute(port_config_flash_write, (void*)config, 5000);
    if (result != 0) {
        printf("[CONFIG] ERROR: flash save failed (%d)\n", result);
        return false;
    }
    return true;
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
