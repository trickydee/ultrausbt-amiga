/*
 * Copyright (c) 2026 ultrausbt
 * https://github.com/trickydee/ultrausbt-amiga
 * SPDX-License-Identifier: EPL-2.0
 * Temporary passive observation of BTstack events, independent of BP32 filtering.
 */
#include "config.h"
#if BT_LOGI_ADV_DEBUG
#include <btstack.h>
#include <ble/le_device_db.h>
#include <stdio.h>
#include <string.h>

// Bound UART load: four reports/second, repeat identical reports after 5 seconds.
// Cache entries distinguish advertising type and payload, not signal strength.
static struct {
    bool used;
    bd_addr_t addr;
    uint8_t addr_type, event_type, len, data[31];
    uint32_t logged_at;
} cache[32];
static unsigned next_entry, emitted, suppressed;
static uint32_t window_start;
static btstack_packet_callback_registration_t registration;

static void advertisement_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void)channel;
    if (packet_type != HCI_EVENT_PACKET || size < 12 || packet[0] != GAP_EVENT_ADVERTISING_REPORT)
        return;
    uint8_t len = gap_event_advertising_report_get_data_length(packet);
    if (len > 31 || size < 12u + len)
        return;
    const uint8_t *data = gap_event_advertising_report_get_data(packet);
    uint8_t type = gap_event_advertising_report_get_address_type(packet);
    uint8_t event = gap_event_advertising_report_get_advertising_event_type(packet);
    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    uint32_t now = btstack_run_loop_get_time_ms();
    if ((uint32_t)(now - window_start) >= 1000) {
        if (suppressed)
            printf("[BLE-ADV] rate-limited=%u reports (capture incomplete)\n", suppressed);
        suppressed = emitted = 0;
        window_start = now;
    }
    unsigned slot = 32;
    for (unsigned i = 0; i < 32; i++) {
        if (cache[i].used && cache[i].addr_type == type && cache[i].event_type == event &&
            memcmp(cache[i].addr, addr, sizeof(addr)) == 0 && cache[i].len == len &&
            memcmp(cache[i].data, data, len) == 0) {
            if ((uint32_t)(now - cache[i].logged_at) < 5000)
                return;
            slot = i;
            break;
        }
    }
    if (emitted >= 4) { suppressed++; return; }
    emitted++;
    if (slot == 32) slot = next_entry++ % 32;
    cache[slot].used = true;
    memcpy(cache[slot].addr, addr, sizeof(addr));
    cache[slot].addr_type = type;
    cache[slot].event_type = event;
    cache[slot].len = len;
    memcpy(cache[slot].data, data, len);
    cache[slot].logged_at = now;

    uint16_t appearance = 0;
    char name[32] = "";
    bool malformed = false;
    for (unsigned off = 0; off < len;) {
        unsigned field_len = data[off];
        if (!field_len) break;
        if (off + 1 + field_len > len) { malformed = true; break; }
        uint8_t ad_type = data[off + 1];
        if (ad_type == 0x19 && field_len >= 3)
            appearance = little_endian_read_16(data, off + 2);
        if (ad_type == 0x08 || ad_type == 0x09) {
            unsigned n = field_len - 1;
            if (n >= sizeof(name)) n = sizeof(name) - 1;
            for (unsigned j = 0; j < n; j++) {
                uint8_t ch = data[off + 2 + j];
                name[j] = ch >= 32 && ch <= 126 ? ch : '.';
            }
            name[n] = 0;
        }
        off += field_len + 1;
    }
    // Exact identity-address match only. No match does NOT mean unbonded:
    // resolving private addresses needs the security manager's IRK resolution.
    int bond = -1;
    for (int i = 0; i < le_device_db_max_count(); i++) {
        int stored_type;
        bd_addr_t stored_addr;
        le_device_db_info(i, &stored_type, stored_addr, NULL);
        if (stored_type == type && memcmp(stored_addr, addr, sizeof(addr)) == 0) {
            bond = i;
            break;
        }
    }
    bool hid = appearance == 0x03c1 || appearance == 0x03c2 ||
               appearance == 0x03c3 || appearance == 0x03c4;
    char hex[63];
    static const char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < len; i++) {
        hex[2*i] = digits[data[i] >> 4];
        hex[2*i+1] = digits[data[i] & 15];
    }
    hex[2*len] = 0;
    printf("[BLE-ADV] t=%lu addr=%s addr_type=%u event=%u rssi=%d appearance=0x%04x appearance_filter=%s exact_bond=%d malformed=%u name='%s' data=%s\n",
           (unsigned long)now, bd_addr_to_str(addr), type, event,
           (int)(int8_t)gap_event_advertising_report_get_rssi(packet), appearance,
           hid ? "pass" : "reject", bond, malformed, name, hex);
}
#endif

void bluepad32_adv_debug_init(void) {
#if BT_LOGI_ADV_DEBUG
    memset(cache, 0, sizeof(cache));
    next_entry = emitted = suppressed = 0;
    window_start = btstack_run_loop_get_time_ms();
    registration.callback = advertisement_event;
    hci_add_event_handler(&registration);
    printf("[BT-TEST] BLE advertisement observer enabled (4 reports/s, identical reports every 5s); passive scan unchanged\n");
#endif
}
