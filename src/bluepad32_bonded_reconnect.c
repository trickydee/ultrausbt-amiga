/* Copyright (c) 2026 ultrausbt — https://github.com/trickydee/ultrausbt-amiga
 * SPDX-License-Identifier: EPL-2.0
 */
#include <btstack.h>
#include <ble/le_device_db.h>
#include <btstack_tlv.h>
#include <stdio.h>
#include <string.h>
#include <bt/uni_bt_allowlist.h>
#include <bt/uni_bt_defines.h>
#include <platform/uni_platform.h>
#include <uni_log.h>
#include "bluepad32_bonded_reconnect.h"
#include "ble_hid_appearance.h"

#ifndef BLUEPAD32_NATIVE_RECONNECT
bool ultrausbt_bonded_directed_advertisement(const uint8_t *packet) {
    // Only the observed ADV_DIRECT_IND case. Do not bypass discovery for an
    // unknown advertiser or attempt to match random addresses by name/prefix.
    if (gap_event_advertising_report_get_advertising_event_type(packet) != 1) return false;
    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    int type = gap_event_advertising_report_get_address_type(packet);
    for (int i = 0; i < le_device_db_max_count(); i++) {
        int stored_type;
        bd_addr_t stored_addr;
        le_device_db_info(i, &stored_type, stored_addr, NULL);
        if (stored_type == type && memcmp(addr, stored_addr, sizeof(addr)) == 0) return true;
    }
    return false;
}

bool ultrausbt_bonded_discovery_allowed(bd_addr_t addr, uint8_t rssi) {
    // Same admission checks as uni_hid_device_on_device_discovered, except COD:
    // directed advertisements carry none. Recover it from the HID descriptor.
    // Upstream keeps this threshold private in uni_hid_device.c. Preserve its
    // existing unsigned-RSSI policy for this narrowly scoped exception.
    const uint8_t discovery_rssi_threshold = 255 - 100;
    if (!uni_bt_allowlist_is_allowed_addr(addr) || rssi < discovery_rssi_threshold) return false;
    struct uni_platform *platform = uni_get_platform();
    return !platform->on_device_discovered ||
        platform->on_device_discovered(addr, "", 0, rssi) == UNI_ERROR_SUCCESS;
}

bool ultrausbt_classify_bonded_hid(uni_hid_device_t *device, uint16_t hids_cid, uint8_t services) {
    uint16_t appearance = 0;
    for (uint8_t i = 0; i < services; i++) {
        const uint8_t *data = hids_client_descriptor_storage_get_descriptor_data(hids_cid, i);
        uint16_t len = hids_client_descriptor_storage_get_descriptor_len(hids_cid, i);
        if (!data || !len) return false;
        uint16_t found = ble_hid_appearance(data, len);
        if (!found || (appearance && found != appearance)) return false;
        appearance = found;
    }
    uint32_t cod;
    const char *name;
    switch (appearance) {
        case 0x03c1: cod = UNI_BT_COD_MINOR_KEYBOARD; name = "BLE keyboard"; break;
        case 0x03c2: cod = UNI_BT_COD_MINOR_MICE; name = "BLE mouse"; break;
        case 0x03c3: cod = UNI_BT_COD_MINOR_JOYSTICK; name = "BLE joystick"; break;
        case 0x03c4: cod = UNI_BT_COD_MINOR_GAMEPAD; name = "BLE gamepad"; break;
        default: return false;
    }
    uni_hid_device_set_cod(device, UNI_BT_COD_MAJOR_PERIPHERAL | cod);
    if (!device->name[0]) uni_hid_device_set_name(device, name);
    logi("[BLE-RECONNECT] HID descriptor classified %s as %s (appearance=0x%04x)\n",
         bd_addr_to_str(device->conn.btaddr), name, appearance);
    return true;
}

#endif

// Display metadata in the existing BTstack flash store, keyed by bond slot.
// Verify the full identity on reads: a recycled bond slot must not inherit a name.
// UBN tags are private to ultrausbt; they contain no authentication material.
#define BLE_NAME_TAG_BASE 0x55424e00u
struct ble_name_record {
    uint8_t version, address_type;
    bd_addr_t address;
    char name[32];
};

static int ble_name_identity(uni_hid_device_t *device, struct ble_name_record *record) {
    int index = sm_le_device_index(device->conn.handle);
    if (index < 0 || index >= le_device_db_max_count() || index > 255) return -1;
    int type;
    le_device_db_info(index, &type, record->address, NULL);
    if (type != BD_ADDR_TYPE_LE_PUBLIC && type != BD_ADDR_TYPE_LE_RANDOM) return -1;
    record->version = 1;
    record->address_type = (uint8_t)type;
    return index;
}

void ultrausbt_restore_ble_name(uni_hid_device_t *device) {
    struct ble_name_record identity = {0}, saved = {0};
    int index = ble_name_identity(device, &identity);
    if (index < 0) return;
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    int size = tlv->get_tag(context, BLE_NAME_TAG_BASE | (uint32_t)index,
                            (uint8_t *)&saved, sizeof(saved));
    if (size != sizeof(saved) || saved.version != identity.version ||
        saved.address_type != identity.address_type ||
        memcmp(saved.address, identity.address, sizeof(saved.address)) != 0 ||
        !saved.name[0] || !memchr(saved.name, 0, sizeof(saved.name))) return;
    uni_hid_device_set_name(device, saved.name);
}

void ultrausbt_save_ble_name(uni_hid_device_t *device) {
    // Never replace a useful saved name with the generic classification fallback.
    if (!device->name[0] || strcmp(device->name, "BLE mouse") == 0 ||
        strcmp(device->name, "BLE keyboard") == 0 || strcmp(device->name, "BLE joystick") == 0 ||
        strcmp(device->name, "BLE gamepad") == 0) return;
    struct ble_name_record record = {0}, saved = {0};
    int index = ble_name_identity(device, &record);
    if (index < 0) return;
    snprintf(record.name, sizeof(record.name), "%.*s", (int)sizeof(record.name) - 1, device->name);
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    uint32_t tag = BLE_NAME_TAG_BASE | (uint32_t)index;
    int size = tlv->get_tag(context, tag, (uint8_t *)&saved, sizeof(saved));
    if (size == sizeof(saved) && memcmp(&saved, &record, sizeof(record)) == 0) return;
    if (tlv->store_tag(context, tag, (const uint8_t *)&record, sizeof(record)))
        loge("[BLE-RECONNECT] Could not persist display name\n");
}

void ultrausbt_clear_ble_names(void) {
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    for (int i = 0; i < le_device_db_max_count() && i <= 255; i++)
        tlv->delete_tag(context, BLE_NAME_TAG_BASE | (uint32_t)i);
}
