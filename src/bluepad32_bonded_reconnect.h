/* Copyright (c) 2026 ultrausbt — https://github.com/trickydee/ultrausbt-amiga
 * SPDX-License-Identifier: EPL-2.0
 */
#ifndef ULTRAUSBT_BONDED_RECONNECT_H
#define ULTRAUSBT_BONDED_RECONNECT_H
#include <uni_hid_device.h>
bool ultrausbt_bonded_directed_advertisement(const uint8_t *packet);
bool ultrausbt_bonded_discovery_allowed(bd_addr_t addr, uint8_t rssi);
bool ultrausbt_classify_bonded_hid(uni_hid_device_t *device, uint16_t hids_cid, uint8_t services);
void ultrausbt_restore_ble_name(uni_hid_device_t *device);
void ultrausbt_save_ble_name(uni_hid_device_t *device);
void ultrausbt_clear_ble_names(void);
#endif
