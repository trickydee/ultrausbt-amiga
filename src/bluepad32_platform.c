/**
 * this file is part of amigahid-pico, (c) 2024
 * please locate the full source at https://github.com/borb/amigahid-pico
 *
 * released under the terms of the Eclipse Public License 2.0 (EPL-2.0).
 * please find the complete license text at https://spdx.org/licenses/EPL-2.0
 *
 * bluepad32 custom platform implementation for amigahid-pico
 * based on bluepad32/examples/pico_w/src/my_platform.c
 * simplified version focused on keyboard support
 */

#if ENABLE_BLUEPAD32

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <uni.h>
#include <string.h>

#include "sdkconfig.h"
#include "platform/amiga/quad_mouse.h"  // For Core 1 pause/resume functions

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico 2 W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

// Maximum number of Bluetooth keyboards we can track
#define MAX_BT_KEYBOARDS 2

// Maximum number of Bluetooth mice we can track
#define MAX_BT_MICE 2

// Maximum number of Bluetooth gamepads we can track
#define MAX_BT_GAMEPADS 2  // First gamepad -> Port 2, second gamepad -> Port 1 (when in joystick mode)

// Storage for Bluetooth keyboard data
typedef struct {
    uni_keyboard_t keyboard;
    bool connected;
    bool updated;  // Set to true when new data arrives
} bt_keyboard_storage_t;

// Storage for Bluetooth mouse data
typedef struct {
    uni_mouse_t mouse;
    bool connected;
    bool updated;  // Set to true when new data arrives
} bt_mouse_storage_t;

// Storage for Bluetooth gamepad data
typedef struct {
    uni_gamepad_t gamepad;
    bool connected;
    bool updated;  // Set to true when new data arrives
} bt_gamepad_storage_t;

static bt_keyboard_storage_t bt_keyboards[MAX_BT_KEYBOARDS] = {0};
static bt_mouse_storage_t bt_mice[MAX_BT_MICE] = {0};
static bt_gamepad_storage_t bt_gamepads[MAX_BT_GAMEPADS] = {0};

// Store device pointer to slot mapping for keyboards, mice, and gamepads
static uni_hid_device_t* keyboard_device_map[MAX_BT_KEYBOARDS] = {0};
static uni_hid_device_t* mouse_device_map[MAX_BT_MICE] = {0};
static uni_hid_device_t* gamepad_device_map[MAX_BT_GAMEPADS] = {0};

// Find the first available slot for a device type, or find existing slot if device already mapped
static int find_slot(uni_hid_device_t* d, uni_hid_device_t** device_map, int max_slots) {
    // First, check if device is already mapped
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == d) {
            return i;
        }
    }
    // Find first free slot
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == NULL) {
            device_map[i] = d;
            return i;
        }
    }
    return -1;  // No free slot
}

// Clear slot when device disconnects
static void clear_slot(uni_hid_device_t* d, uni_hid_device_t** device_map, int max_slots) {
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == d) {
            device_map[i] = NULL;
            break;
        }
    }
}

static bt_keyboard_storage_t* get_keyboard_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, keyboard_device_map, MAX_BT_KEYBOARDS);
    if (idx >= 0) {
        return &bt_keyboards[idx];
    }
    return NULL;
}

static bt_mouse_storage_t* get_mouse_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, mouse_device_map, MAX_BT_MICE);
    if (idx >= 0) {
        return &bt_mice[idx];
    }
    return NULL;
}

static bt_gamepad_storage_t* get_gamepad_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, gamepad_device_map, MAX_BT_GAMEPADS);
    if (idx >= 0) {
        return &bt_gamepads[idx];
    }
    return NULL;
}

// Platform Overrides
static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("bluepad32_platform: init()\n");
}

static void my_platform_on_init_complete(void) {
    logi("bluepad32_platform: on_init_complete()\n");

    // Wait a bit for HCI to be ready
    logi("Waiting for HCI to be ready...\n");
    sleep_ms(2000);  // Give HCI 2 seconds to initialize

    // Start scanning and autoconnect to supported devices
    logi("Starting Bluetooth scanning and autoconnect...\n");
    uni_bt_start_scanning_and_autoconnect_unsafe();
    logi("Bluetooth scanning started - waiting for devices...\n");
    logi("Put your keyboard in pairing mode now!\n");

    // Turn off LED once init is done
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    logi("BT Device discovered: addr=%s, name='%s', COD=0x%04X, RSSI=%d\n",
         addr_str, name ? name : "(null)", cod, rssi);
    
    // Pause Core 1 immediately when a gamepad is discovered to prevent freeze during GATT service discovery
    // COD 0x0508 = Gamepad/Joystick class
    bool might_be_gamepad = (cod == 0x0508) ||  // Gamepad COD
                            (name != NULL && (strstr(name, "Stadia") != NULL || 
                                              strstr(name, "Xbox") != NULL ||
                                              strstr(name, "XBOX") != NULL ||
                                              strstr(name, "gamepad") != NULL ||
                                              strstr(name, "Gamepad") != NULL ||
                                              strstr(name, "GAMEPAD") != NULL));
    
    if (might_be_gamepad) {
        logi("[DIAG] Pausing Core 1 for gamepad device discovery (COD=0x%04X, name='%s')\n", 
             cod, name ? name : "(null)");
        amiga_quad_mouse_pause_core1();
    }
    
    // Accept all HID devices (keyboards, mice, gamepads)
    logi("  -> Accepting device (will attempt connection)\n");
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("bluepad32_platform: device connected: %p\n", d);
    
    // Check if this might be a gamepad and ensure Core 1 is paused
    // We check vendor ID if available (Xbox = 0x045E, Google/Stadia = 0x18D1)
    uint16_t vendor_id = uni_hid_device_get_vendor_id(d);
    bool might_be_gamepad = (vendor_id == 0x045E) ||  // Microsoft (Xbox)
                            (vendor_id == 0x18D1);    // Google (Stadia)
    
    // Ensure Core 1 is paused (may have been paused earlier during discovery)
    // The freeze happens during GATT service discovery which occurs here
    if (might_be_gamepad) {
        logi("[DIAG] Ensuring Core 1 is paused for gamepad device connection\n");
        amiga_quad_mouse_pause_core1();
    }
    
    // Device type will be determined in on_device_ready()
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("bluepad32_platform: device disconnected: %p\n", d);
    
    // Ensure Core 1 is resumed if device disconnects during enumeration
    // (safety check in case resume wasn't called)
    amiga_quad_mouse_resume_core1();
    
    // Clear keyboard storage if it was a keyboard
    bt_keyboard_storage_t* kb_storage = get_keyboard_storage(d);
    if (kb_storage && kb_storage->connected) {
        kb_storage->connected = false;
        kb_storage->updated = false;
        memset(&kb_storage->keyboard, 0, sizeof(kb_storage->keyboard));
        clear_slot(d, keyboard_device_map, MAX_BT_KEYBOARDS);
        logi("bluepad32_platform: keyboard disconnected\n");
    }
    
    // Clear mouse storage if it was a mouse
    bt_mouse_storage_t* mouse_storage = get_mouse_storage(d);
    if (mouse_storage && mouse_storage->connected) {
        mouse_storage->connected = false;
        mouse_storage->updated = false;
        memset(&mouse_storage->mouse, 0, sizeof(mouse_storage->mouse));
        clear_slot(d, mouse_device_map, MAX_BT_MICE);
        logi("bluepad32_platform: mouse disconnected\n");
    }
    
    // Clear gamepad storage if it was a gamepad
    bt_gamepad_storage_t* gamepad_storage = get_gamepad_storage(d);
    if (gamepad_storage && gamepad_storage->connected) {
        gamepad_storage->connected = false;
        gamepad_storage->updated = false;
        memset(&gamepad_storage->gamepad, 0, sizeof(gamepad_storage->gamepad));
        clear_slot(d, gamepad_device_map, MAX_BT_GAMEPADS);
        logi("bluepad32_platform: gamepad disconnected\n");
    }
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("bluepad32_platform: device ready: %p\n", d);
    
    // Determine device type and mark appropriate storage as connected
    if (uni_hid_device_is_keyboard(d)) {
        // Keyboard - mark as connected
        bt_keyboard_storage_t* storage = get_keyboard_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
        }
        logi("bluepad32_platform: keyboard ready\n");
    } else if (uni_hid_device_is_mouse(d)) {
        // Mouse - mark as connected
        bt_mouse_storage_t* storage = get_mouse_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
        }
        logi("bluepad32_platform: mouse ready\n");
    } else if (uni_hid_device_is_gamepad(d)) {
        // Gamepad - mark as connected (first one mapped to joystick port 2)
        bt_gamepad_storage_t* storage = get_gamepad_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            logi("bluepad32_platform: gamepad ready\n");
        } else {
            logi("bluepad32_platform: gamepad ready but no storage slot available (MAX_BT_GAMEPADS=%d)\n", MAX_BT_GAMEPADS);
        }
        
        // Add 10ms delay after gamepad enumeration to prevent lockup
        // This is needed to allow GATT service discovery to complete properly
        // Reference: Atari keyboard interface fix for similar enumeration issues
        logi("[DIAG] Waiting 10ms before resuming Core 1 after gamepad enumeration...\n");
        sleep_ms(10);
        logi("[DIAG] Resuming Core 1 after gamepad enumeration\n");
        amiga_quad_mouse_resume_core1();
    } else {
        logi("bluepad32_platform: device type not supported\n");
    }
    
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_KEYBOARD: {
            bt_keyboard_storage_t* storage = get_keyboard_storage(d);
            if (storage) {
                if (!storage->connected) {
                    // First keyboard data - mark as connected
                    storage->connected = true;
                }
                // Copy keyboard data
                storage->keyboard = ctl->keyboard;
                storage->updated = true;
            }
            break;
        }
        
        case UNI_CONTROLLER_CLASS_MOUSE: {
            bt_mouse_storage_t* storage = get_mouse_storage(d);
            if (storage) {
                if (!storage->connected) {
                    // First mouse data - mark as connected
                    storage->connected = true;
                }
                // Copy mouse data
                storage->mouse = ctl->mouse;
                storage->updated = true;
            }
            break;
        }
        
        case UNI_CONTROLLER_CLASS_GAMEPAD: {
            bt_gamepad_storage_t* storage = get_gamepad_storage(d);
            // Only update if already marked as connected in on_device_ready()
            // This prevents processing gamepad data before device enumeration is complete
            if (storage && storage->connected) {
                // Copy gamepad data
                storage->gamepad = ctl->gamepad;
                storage->updated = true;
            }
            break;
        }
        
        default:
            // Ignore other controller types
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(data);
    
    switch (event) {
        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
            logi("bluepad32_platform: Bluetooth enabled: %d\n", (bool)(data));
            break;
        default:
            // Ignore other events
            break;
    }
}

// Entry Point
struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "Amiga HID Platform",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };

    return &plat;
}

// Public API to get Bluetooth keyboard data
// Returns true if keyboard is connected and has data
bool bluepad32_get_keyboard(int idx, void* out_keyboard) {
    if (idx < 0 || idx >= MAX_BT_KEYBOARDS || !out_keyboard) {
        return false;
    }
    
    if (bt_keyboards[idx].connected && bt_keyboards[idx].updated) {
        // Copy the keyboard data (caller's struct must match uni_keyboard_t layout)
        uni_keyboard_t* kb = (uni_keyboard_t*)out_keyboard;
        *kb = bt_keyboards[idx].keyboard;
        bt_keyboards[idx].updated = false;  // Mark as read
        return true;
    }
    
    return false;
}

// Peek at keyboard data without marking as read (for shortcuts that need to check state)
bool bluepad32_peek_keyboard(int idx, void* out_keyboard) {
    if (idx < 0 || idx >= MAX_BT_KEYBOARDS || !out_keyboard) {
        return false;
    }
    
    if (bt_keyboards[idx].connected) {
        // Copy the keyboard data without clearing the updated flag
        uni_keyboard_t* kb = (uni_keyboard_t*)out_keyboard;
        *kb = bt_keyboards[idx].keyboard;
        return true;
    }
    
    return false;
}

// Get count of connected Bluetooth keyboards
int bluepad32_get_keyboard_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_BT_KEYBOARDS; i++) {
        if (bt_keyboards[i].connected) {
            count++;
        }
    }
    return count;
}

// Public API to get Bluetooth mouse data
// Returns true if mouse is connected and has data
bool bluepad32_get_mouse(int idx, void* out_mouse) {
    if (idx < 0 || idx >= MAX_BT_MICE || !out_mouse) {
        return false;
    }
    
    if (bt_mice[idx].connected && bt_mice[idx].updated) {
        // Copy the mouse data (caller's struct must match uni_mouse_t layout)
        uni_mouse_t* mouse = (uni_mouse_t*)out_mouse;
        *mouse = bt_mice[idx].mouse;
        bt_mice[idx].updated = false;  // Mark as read
        return true;
    }
    
    return false;
}

// Get count of connected Bluetooth mice
int bluepad32_get_mouse_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_BT_MICE; i++) {
        if (bt_mice[i].connected) {
            count++;
        }
    }
    return count;
}

// Public API to get Bluetooth gamepad data
bool bluepad32_get_gamepad(int idx, void* out_gamepad) {
    if (idx < 0 || idx >= MAX_BT_GAMEPADS || !out_gamepad) {
        return false;
    }
    
    if (bt_gamepads[idx].connected && bt_gamepads[idx].updated) {
        // Copy the gamepad data (caller's struct must match uni_gamepad_t layout)
        uni_gamepad_t* gamepad = (uni_gamepad_t*)out_gamepad;
        *gamepad = bt_gamepads[idx].gamepad;
        bt_gamepads[idx].updated = false;  // Mark as read
        return true;
    }
    
    return false;
}

// Get count of connected Bluetooth gamepads
int bluepad32_get_gamepad_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_BT_GAMEPADS; i++) {
        if (bt_gamepads[i].connected) {
            count++;
        }
    }
    return count;
}

// Delete all stored Bluetooth pairing keys
void bluepad32_delete_pairing_keys(void) {
    uni_bt_del_keys_unsafe();
}

#endif // ENABLE_BLUEPAD32

