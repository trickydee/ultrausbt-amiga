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

#include "config.h"
#include "sdkconfig.h"
#include "platform/amiga/quad_mouse.h"
#include "display/display.h"
#include "hardware/sync.h"

// busy_wait_us is safe inside BT callbacks; sleep_ms/__wfe are not (timer/IRQ may stall).
static void bt_callback_busy_wait_ms(uint32_t ms)
{
    busy_wait_us(ms * 1000u);
}

static bool might_be_bt_gamepad(uint16_t cod, const char* name)
{
    return (cod == 0x0508) ||
           (name != NULL && (strstr(name, "Stadia") != NULL ||
                             strstr(name, "Xbox") != NULL ||
                             strstr(name, "XBOX") != NULL));
}

static void bt_resume_core1_if_paused(void)
{
    if (core1_get_bt_pause_depth() > 0) {
        bt_callback_busy_wait_ms(BT_GAMEPAD_CORE1_RESUME_DELAY_MS);
        core1_resume_after_bt_enumeration();
    }
}

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

    logi("Waiting for HCI to be ready...\n");
    bt_callback_busy_wait_ms(2000);

    bluepad32_pairing_start();
    g_pairing_boot_window_active = true;
    g_pairing_boot_deadline = make_timeout_time_ms(PAIRING_BOOT_WINDOW_MS);

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    char addr_str[18];
    snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    logi("BT Device discovered: addr=%s, name='%s', COD=0x%04X, RSSI=%d\n",
         addr_str, name ? name : "(null)", cod, rssi);

    if (might_be_bt_gamepad(cod, name)) {
        logi("[DIAG] Pausing Core 1 for gamepad discovery (COD=0x%04X, name='%s')\n",
             cod, name ? name : "(null)");
        core1_pause_for_bt_enumeration();
        core1_wait_for_pause_active(20);
        bt_callback_busy_wait_ms(BT_GAMEPAD_DISCOVERY_SETTLE_MS);
    }

    if (name && name[0] != '\0') {
        store_pending_name_by_addr(addr, name);
    }

    logi("  -> Accepting device (will attempt connection)\n");
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    logi("bluepad32_platform: device connected: %p\n", d);
    // Discovery already pauses Core 1 for gamepads — do not pause again here.
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("bluepad32_platform: device disconnected: %p\n", d);

    if (core1_get_bt_pause_depth() > 0) {
        logi("[DIAG] disconnect during enumeration (depth=%lu), resuming Core 1\n",
             (unsigned long)core1_get_bt_pause_depth());
        core1_resume_after_bt_enumeration();
    }
    
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
    } else if (uni_hid_device_is_gamepad(d)) {
        bt_gamepad_storage_t* storage = get_gamepad_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
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
    } else {
        logi("bluepad32_platform: device type not supported\n");
    }

#if HIDPICO_REVISION == 5
    update_bt_device_counts();
#endif

    if (core1_get_bt_pause_depth() > 0) {
        logi("[DIAG] Waiting %dms before resuming Core 1 after device ready (depth=%lu)\n",
             BT_GAMEPAD_CORE1_RESUME_DELAY_MS,
             (unsigned long)core1_get_bt_pause_depth());
        bt_resume_core1_if_paused();
        logi("[DIAG] Core 1 BT pause depth after ready: %lu\n",
             (unsigned long)core1_get_bt_pause_depth());
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
    core1_force_release_bt_pause();
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

