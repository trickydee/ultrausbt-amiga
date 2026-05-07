/**
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
#include <bt/uni_bt.h>
#include <string.h>

#include "sdkconfig.h"
#include "controller/uni_controller_type.h"  // For controller type enums (Xbox, Stadia, etc.)
#include "platform/amiga/quad_mouse.h"  // For Core 1 pause/resume functions
#include "display/display.h"

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
    char name[32];  // Device name (null-terminated)
} bt_keyboard_storage_t;

// Storage for Bluetooth mouse data
typedef struct {
    uni_mouse_t mouse;
    bool connected;
    bool updated;  // Set to true when new data arrives
    char name[32];  // Device name (null-terminated)
} bt_mouse_storage_t;

// Storage for Bluetooth gamepad data
typedef struct {
    uni_gamepad_t gamepad;
    bool connected;
    bool updated;  // Set to true when new data arrives
    char name[32];  // Device name (null-terminated)
} bt_gamepad_storage_t;

static bt_keyboard_storage_t bt_keyboards[MAX_BT_KEYBOARDS] = {0};
static bt_mouse_storage_t bt_mice[MAX_BT_MICE] = {0};
static bt_gamepad_storage_t bt_gamepads[MAX_BT_GAMEPADS] = {0};
static bool g_pairing_active = false;
static bool g_pairing_boot_window_active = false;
static absolute_time_t g_pairing_boot_deadline;
static const uint32_t PAIRING_BOOT_WINDOW_MS = 60000;

void bluepad32_pairing_start(void);
void bluepad32_pairing_stop(void);

// Store device pointer to slot mapping for keyboards, mice, and gamepads
static uni_hid_device_t* keyboard_device_map[MAX_BT_KEYBOARDS] = {0};
static uni_hid_device_t* mouse_device_map[MAX_BT_MICE] = {0};
static uni_hid_device_t* gamepad_device_map[MAX_BT_GAMEPADS] = {0};

// Store device names discovered by address (before we have device pointer)
// Map by Bluetooth address (6 bytes) so we can match them when device is ready
#define MAX_PENDING_NAMES_BY_ADDR 8
typedef struct {
    bd_addr_t addr;
    char name[32];
    bool valid;
} pending_name_by_addr_t;
static pending_name_by_addr_t pending_names_by_addr[MAX_PENDING_NAMES_BY_ADDR] = {0};

static void store_pending_name_by_addr(bd_addr_t addr, const char* name) {
    if (!name || name[0] == '\0') {
        return;
    }
    
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (!pending_names_by_addr[i].valid || 
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            memcpy(pending_names_by_addr[i].addr, addr, 6);
            strncpy(pending_names_by_addr[i].name, name, sizeof(pending_names_by_addr[i].name) - 1);
            pending_names_by_addr[i].name[sizeof(pending_names_by_addr[i].name) - 1] = '\0';
            pending_names_by_addr[i].valid = true;
            return;
        }
    }
}

// Get stored device name by address
static const char* get_pending_name_by_addr(bd_addr_t addr) {
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (pending_names_by_addr[i].valid && 
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            return pending_names_by_addr[i].name;
        }
    }
    return NULL;
}

// Clear pending name when device is ready
static void clear_pending_name_by_addr(bd_addr_t addr) {
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (pending_names_by_addr[i].valid && 
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            pending_names_by_addr[i].valid = false;
            return;
        }
    }
}

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

#if HIDPICO_REVISION == 5
// Forward declaration
static void update_bt_device_counts(void);
#endif

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

    // Pairing is ON for 60 seconds at boot, then auto-locks OFF.
    bluepad32_pairing_start();
    g_pairing_boot_window_active = true;
    g_pairing_boot_deadline = make_timeout_time_ms(PAIRING_BOOT_WINDOW_MS);

    // Turn off LED once init is done
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    logi("BT Device discovered: addr=%s, name='%s', COD=0x%04X, RSSI=%d\n",
         addr_str, name ? name : "(null)", cod, rssi);
    
    // Store device name by address for later matching
    if (name && name[0] != '\0') {
        store_pending_name_by_addr(addr, name);
    }
    
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
    
    // Check if this is an Xbox or Stadia gamepad and ensure Core 1 is paused
    // Use controller type for Xbox (if available), vendor ID for Stadia
    bool is_xbox = false;
    bool is_stadia = false;
    
    // Check controller type for Xbox (bluepad32 identifies this automatically)
    if (uni_hid_device_has_controller_type(d)) {
        uni_controller_type_t ctrl_type = d->controller_type;
        is_xbox = (ctrl_type == k_eControllerType_XBoxOneController) ||
                  (ctrl_type == k_eControllerType_XBox360Controller);
    }
    
    // For Stadia, check vendor ID (Stadia is mapped to AndroidController, so we need VID/PID)
    // Note: In on_device_connected, product_id may not be available yet
    uint16_t vendor_id = uni_hid_device_get_vendor_id(d);
    is_stadia = (vendor_id == 0x18D1);  // Google (Stadia)
    
    bool is_xbox_stadia = is_xbox || is_stadia;
    
    // Ensure Core 1 is paused (may have been paused earlier during discovery)
    // The freeze happens during GATT service discovery which occurs here
    if (is_xbox_stadia) {
        logi("[DIAG] Ensuring Core 1 is paused for Xbox/Stadia device connection\n");
        amiga_quad_mouse_pause_core1();
    }
    
    // Device type will be determined in on_device_ready()
    // Device name will be retrieved from d->name or matched by address
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
        kb_storage->name[0] = '\0';  // Clear name
        clear_slot(d, keyboard_device_map, MAX_BT_KEYBOARDS);
        logi("bluepad32_platform: keyboard disconnected\n");
    }
    
    // Clear mouse storage if it was a mouse
    bt_mouse_storage_t* mouse_storage = get_mouse_storage(d);
    if (mouse_storage && mouse_storage->connected) {
        mouse_storage->connected = false;
        mouse_storage->updated = false;
        memset(&mouse_storage->mouse, 0, sizeof(mouse_storage->mouse));
        mouse_storage->name[0] = '\0';  // Clear name
        clear_slot(d, mouse_device_map, MAX_BT_MICE);
        logi("bluepad32_platform: mouse disconnected\n");
    }
    
    // Clear gamepad storage if it was a gamepad
    bt_gamepad_storage_t* gamepad_storage = get_gamepad_storage(d);
    if (gamepad_storage && gamepad_storage->connected) {
        gamepad_storage->connected = false;
        gamepad_storage->updated = false;
        memset(&gamepad_storage->gamepad, 0, sizeof(gamepad_storage->gamepad));
        gamepad_storage->name[0] = '\0';  // Clear name
        clear_slot(d, gamepad_device_map, MAX_BT_GAMEPADS);
        logi("bluepad32_platform: gamepad disconnected\n");
    }
    
#if HIDPICO_REVISION == 5
    // Update display with new Bluetooth device counts
    update_bt_device_counts();
#endif
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("bluepad32_platform: device ready: %p\n", d);
    
    // Get device address to match with discovered name
    bd_addr_t addr;
    uni_bt_conn_get_address(&d->conn, addr);
    
    // Try to get device name from pending storage (by address)
    const char* stored_name = get_pending_name_by_addr(addr);
    
    // Get device name - prefer bluepad32's stored name, fallback to discovered name, then default
    const char* device_name = NULL;
    if (uni_hid_device_has_name(d) && d->name[0] != '\0') {
        // Bluepad32 has the device name
        device_name = d->name;
    } else if (stored_name && stored_name[0] != '\0') {
        // Use name from discovery
        device_name = stored_name;
    }
    
    // Determine device type and mark appropriate storage as connected
    if (uni_hid_device_is_keyboard(d)) {
        // Keyboard - mark as connected
        bt_keyboard_storage_t* storage = get_keyboard_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            // Store device name
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Keyboard");
            }
        }
        logi("bluepad32_platform: keyboard ready\n");
        
#if HIDPICO_REVISION == 5
        // Update display with new Bluetooth device counts
        update_bt_device_counts();
#endif
    } else if (uni_hid_device_is_mouse(d)) {
        // Mouse - mark as connected
        bt_mouse_storage_t* storage = get_mouse_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            // Store device name
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Mouse");
            }
        }
        logi("bluepad32_platform: mouse ready\n");
        
#if HIDPICO_REVISION == 5
        // Update display with new Bluetooth device counts
        update_bt_device_counts();
#endif
    } else if (uni_hid_device_is_gamepad(d)) {
        // Check if this is an Xbox or Stadia gamepad (known to cause Core 1 freeze)
        // These controllers need special handling during enumeration
        // IMPORTANT: Check vendor/product ID BEFORE allocating storage (matches Atari code ordering)
        bool is_xbox = false;
        bool is_stadia = false;
        
        // Use controller type for Xbox (bluepad32 identifies this automatically)
        if (uni_hid_device_has_controller_type(d)) {
            uni_controller_type_t ctrl_type = d->controller_type;
            is_xbox = (ctrl_type == k_eControllerType_XBoxOneController) ||
                      (ctrl_type == k_eControllerType_XBox360Controller);
        }
        
        // For Stadia, check vendor/product ID (Stadia is mapped to AndroidController)
        uint16_t vendor_id = uni_hid_device_get_vendor_id(d);
        uint16_t product_id = uni_hid_device_get_product_id(d);
        is_stadia = (vendor_id == 0x18D1 && product_id == 0x9400);  // Google (Stadia)
        
        bool is_xbox_stadia = is_xbox || is_stadia;
        
        // Gamepad - mark as connected (first one mapped to joystick port 2)
        bt_gamepad_storage_t* storage = get_gamepad_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            // Store device name
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Gamepad");
            }
            logi("bluepad32_platform: gamepad ready\n");
        } else {
            logi("bluepad32_platform: gamepad ready but no storage slot available (MAX_BT_GAMEPADS=%d)\n", MAX_BT_GAMEPADS);
        }
        
        // Resume Core 1 after a short delay to ensure enumeration completes
        // Note: Core 1 should already be paused from device_discovered/device_connected
        // IMPORTANT: Do NOT update display during Stadia/Xbox enumeration - defer to main loop
        // Display updates can interfere with flash access coordination during enumeration
        if (is_xbox_stadia) {
            // For Stadia/Xbox, use a delay to ensure GATT service discovery completes
            // Stadia controllers are particularly sensitive to timing during enumeration
            // Skip display update during enumeration - main loop will update it later
            logi("[DIAG] Xbox/Stadia detected - waiting 50ms before resuming Core 1 (already paused from discovery)...\n");
            sleep_ms(50);  // 50ms delay - reduced from 100ms for faster resume
            logi("[DIAG] Resuming Core 1 after Xbox/Stadia enumeration\n");
            amiga_quad_mouse_resume_core1();
            
            // Check Core 1 heartbeat to verify it's actually running
            // Declare extern for heartbeat counter from quad_mouse.c
            extern volatile uint32_t g_core1_heartbeat;
            uint32_t heartbeat_before = g_core1_heartbeat;
            
            // Wait a bit and check if Core 1 is actually running
            sleep_ms(50);
            uint32_t heartbeat_after = g_core1_heartbeat;
            if (heartbeat_after > heartbeat_before) {
                logi("[DIAG] Core 1 resume confirmed - heartbeat increased from %lu to %lu\n", heartbeat_before, heartbeat_after);
            } else {
                logi("[DIAG] WARNING: Core 1 may not be resuming - heartbeat unchanged (%lu)\n", heartbeat_before);
            }
            // Display update deferred - will be updated in main loop via periodic checks
        } else {
            // For other gamepads, use shorter delay
            logi("[DIAG] Waiting 10ms before resuming Core 1 after gamepad enumeration...\n");
            sleep_ms(10);
            amiga_quad_mouse_resume_core1();
            logi("[DIAG] Core 1 resume completed for other gamepad\n");
#if HIDPICO_REVISION == 5
            // Update display after resuming Core 1 for non-Stadia gamepads
            update_bt_device_counts();
#endif
        }
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
                    // Update name from device if not already set
                    if (storage->name[0] == '\0' && uni_hid_device_has_name(d) && d->name[0] != '\0') {
                        snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), d->name);
                    } else if (storage->name[0] == '\0') {
                        snprintf(storage->name, sizeof(storage->name), "Keyboard");
                    }
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
                    // Update name from device if not already set
                    if (storage->name[0] == '\0' && uni_hid_device_has_name(d) && d->name[0] != '\0') {
                        snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), d->name);
                    } else if (storage->name[0] == '\0') {
                        snprintf(storage->name, sizeof(storage->name), "Mouse");
                    }
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

#if HIDPICO_REVISION == 5
// Count Bluetooth devices and update display
static void update_bt_device_counts(void)
{
    uint8_t kb_count = 0;
    uint8_t mouse_count = 0;
    uint8_t joy_count = 0;
    
    // Count connected Bluetooth keyboards
    for (int i = 0; i < MAX_BT_KEYBOARDS; i++) {
        if (bt_keyboards[i].connected) {
            kb_count++;
        }
    }
    
    // Count connected Bluetooth mice
    for (int i = 0; i < MAX_BT_MICE; i++) {
        if (bt_mice[i].connected) {
            mouse_count++;
        }
    }
    
    // Count connected Bluetooth gamepads
    for (int i = 0; i < MAX_BT_GAMEPADS; i++) {
        if (bt_gamepads[i].connected) {
            joy_count++;
        }
    }
    
    display_set_bt_counts(kb_count, mouse_count, joy_count);
}
#endif

// Delete all stored Bluetooth pairing keys
void bluepad32_delete_pairing_keys(void) {
    uni_bt_del_keys_unsafe();
}

void bluepad32_pairing_start(void) {
    if (g_pairing_active) {
        return;
    }
    g_pairing_active = true;
    logi("Bluetooth pairing: ON\n");
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

void bluepad32_pairing_stop(void) {
    if (!g_pairing_active) {
        return;
    }
    g_pairing_active = false;
    g_pairing_boot_window_active = false;
    logi("Bluetooth pairing: OFF\n");
    uni_bt_stop_scanning_unsafe();
}

bool bluepad32_pairing_is_active(void) {
    return g_pairing_active;
}

uint32_t bluepad32_pairing_remaining_seconds(void) {
    if (!g_pairing_active || !g_pairing_boot_window_active) {
        return 0;
    }
    int64_t us_left = absolute_time_diff_us(get_absolute_time(), g_pairing_boot_deadline);
    if (us_left <= 0) {
        return 0;
    }
    return (uint32_t)((us_left + 999999) / 1000000);
}

void bluepad32_pairing_tick(void) {
    if (!g_pairing_active || !g_pairing_boot_window_active) {
        return;
    }
    if (absolute_time_diff_us(get_absolute_time(), g_pairing_boot_deadline) <= 0) {
        logi("Bluetooth pairing: boot window expired after %lu seconds\n",
             (unsigned long)(PAIRING_BOOT_WINDOW_MS / 1000));
        bluepad32_pairing_stop();
    }
}

// Get Bluetooth device name for display
// Returns device name or NULL if not available
const char* bluepad32_get_device_name(char device_type, int idx) {
    if (idx < 0 || idx >= 2) {
        return NULL;
    }
    
    switch (device_type) {
        case 'J':  // Joystick/Gamepad
            if (idx < MAX_BT_GAMEPADS && bt_gamepads[idx].connected) {
                return bt_gamepads[idx].name;
            }
            break;
        case 'K':  // Keyboard
            if (idx < MAX_BT_KEYBOARDS && bt_keyboards[idx].connected) {
                return bt_keyboards[idx].name;
            }
            break;
        case 'M':  // Mouse
            if (idx < MAX_BT_MICE && bt_mice[idx].connected) {
                return bt_mice[idx].name;
            }
            break;
        default:
            return NULL;
    }
    
    return NULL;
}

#endif // ENABLE_BLUEPAD32

