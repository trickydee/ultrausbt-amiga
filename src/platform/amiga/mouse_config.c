/**
 * this file is part of amigahid-pico, (c) 2021 just nine <nine@aphlor.org>
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * Mouse configuration persistence using flash storage.
 */

#include "mouse_config.h"
#include "quad_mouse.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"  // For save_and_disable_interrupts()
#include <stdio.h>          // For printf
#include <string.h>

// Flash storage configuration
// CRITICAL: Use the LAST 4KB sector of flash so config never overwrites firmware.
// Pico W / Pico 2 W firmware can be ~1.2–1.3MB; storing at 0x40000 (256KB) was
// inside the firmware region and caused hangs/corruption when saving mouse type.
// Last sector of 2MB flash: 0x200000 - 0x1000 = 0x1FF000
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_MAGIC 0x4D4F5553  // "MOUS" in ASCII

// Configuration data structure
typedef struct {
    uint32_t magic;           // Validation magic number (0x4D4F5553 = "MOUS")
    mouse_type_t mouse_type;  // Current mouse type (0 = Amiga, 1 = Atari)
    uint32_t reserved[14];    // Reserved for future settings (64 bytes total)
} mouse_config_t;

// Static buffer for flash operations (must be aligned to 256 bytes)
static uint8_t __attribute__((aligned(256))) flash_buffer[FLASH_SECTOR_SIZE];

/**
 * Load mouse configuration from flash storage.
 * Returns the saved mouse type, or MOUSE_TYPE_AMIGA if no valid config found.
 */
mouse_type_t mouse_config_load(void)
{
    // Flash is memory-mapped, so we can read directly
    const mouse_config_t *config = (const mouse_config_t *)(XIP_BASE + CONFIG_FLASH_OFFSET);
    
    // Validate magic number
    if (config->magic == CONFIG_MAGIC) {
        // Validate mouse type value
        if (config->mouse_type == MOUSE_TYPE_AMIGA || config->mouse_type == MOUSE_TYPE_ATARI) {
            printf("[CONFIG] Loaded mouse type: %s\n", 
                   config->mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
            return config->mouse_type;
        }
    }
    
    // No valid config found - return default
    printf("[CONFIG] No valid config found, using default (Amiga)\n");
    return MOUSE_TYPE_AMIGA;
}

/**
 * Save mouse configuration to flash storage.
 * Returns true on success, false on failure.
 * 
 * NOTE: This function uses flash_safe_execute to coordinate with Core 1
 * to prevent conflicts with flash_safe_execute coordination.
 */
// Flash operation helper - must be called from flash_safe_execute context
static void mouse_config_flash_write(void* param)
{
    mouse_type_t mouse_type = (mouse_type_t)(uintptr_t)param;
    mouse_config_t *config = (mouse_config_t *)flash_buffer;
    
    printf("[CONFIG] Preparing flash buffer...\n");
    // Prepare configuration data
    memset(flash_buffer, 0xFF, FLASH_SECTOR_SIZE);  // Erase pattern (all 1s)
    config->magic = CONFIG_MAGIC;
    config->mouse_type = mouse_type;
    // Reserved fields remain 0xFF (erased state)
    
    printf("[CONFIG] Erasing flash sector at 0x%x...\n", CONFIG_FLASH_OFFSET);
    // Erase flash sector and write new data
    // Disable interrupts during flash operations
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    
    printf("[CONFIG] Programming flash...\n");
    ints = save_and_disable_interrupts();
    flash_range_program(CONFIG_FLASH_OFFSET, flash_buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

bool mouse_config_save(mouse_type_t mouse_type)
{
    printf("[CONFIG] Starting save: mouse_type=%d (%s)\n", 
           mouse_type, mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
    
    // CRITICAL: Use flash_safe_execute to coordinate with Core 1
    // Core 1 uses flash_safe_execute_core_init() for Bluetooth flash operations
    // Using flash_safe_execute() ensures proper coordination and prevents deadlocks
    printf("[CONFIG] Executing flash write with Core 1 coordination...\n");
    
    // Use a longer timeout (5 seconds) to allow Core 1 to finish any ongoing flash operations
    // This is especially important during Bluetooth pairing when Core 1 may be in flash_safe_execute
    int result = flash_safe_execute(mouse_config_flash_write, (void*)(uintptr_t)mouse_type, 5000);
    
    if (result != 0) {
        printf("[CONFIG] ERROR: Flash write failed with code %d (timeout or error)\n", result);
        printf("[CONFIG] This may indicate Core 1 is stuck in a flash operation\n");
        return false;
    }
    
    printf("[CONFIG] Flash write complete\n");
    printf("[CONFIG] Saved mouse type: %s\n", 
           mouse_type == MOUSE_TYPE_ATARI ? "Atari" : "Amiga");
    return true;
}

