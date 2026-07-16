#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_STATE_DISABLED,
    BT_STATE_IDLE,
    BT_STATE_SCANNING,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
    BT_STATE_DISCONNECTING
} bt_state_t;

typedef struct {
    char name[64];
    uint8_t addr[6];
    int8_t rssi;
    uint8_t addr_type;
    uint8_t bond_status;
} bt_device_t;

typedef struct {
    uint8_t addr[6];
    char name[64];
} bt_bonded_device_t;

typedef void (*bt_state_cb_t)(bt_state_t state);
typedef void (*bt_device_found_cb_t)(bt_device_t *device);
typedef void (*bt_connection_cb_t)(bool connected, uint8_t *addr);
typedef void (*bt_scan_complete_cb_t)(void);

esp_err_t bt_manager_init(void);
esp_err_t bt_manager_enable(void);
esp_err_t bt_manager_disable(void);
bt_state_t bt_manager_get_state(void);
esp_err_t bt_manager_start_scan(void);
esp_err_t bt_manager_stop_scan(void);
esp_err_t bt_manager_connect(const uint8_t *addr, uint8_t addr_type);
esp_err_t bt_manager_disconnect(void);
esp_err_t bt_manager_remove_bond(const uint8_t *addr);
int bt_manager_get_bonded_devices(bt_bonded_device_t *devices, int max_count);
void bt_manager_register_state_cb(bt_state_cb_t cb);
void bt_manager_register_device_found_cb(bt_device_found_cb_t cb);
void bt_manager_register_connection_cb(bt_connection_cb_t cb);
void bt_manager_register_scan_complete_cb(bt_scan_complete_cb_t cb);

#ifdef __cplusplus
}
#endif
