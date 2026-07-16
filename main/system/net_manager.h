#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_FAILED
} net_state_t;

typedef enum {
    NET_TYPE_NONE,
    NET_TYPE_WIFI,
    NET_TYPE_ETHERNET
} net_type_t;

typedef struct {
    net_type_t type;
    net_state_t state;
    char ssid[32];
    char ip[16];
    int rssi;
} net_info_t;

typedef struct {
    char ssid[32];
    int rssi;
    int channel;
} net_ap_info_t;

#define MAX_AP_RECORDS 20

esp_err_t net_manager_init(void);
esp_err_t net_manager_deinit(void);

esp_err_t net_manager_wifi_connect(const char *ssid, const char *password);
esp_err_t net_manager_wifi_disconnect(void);
int net_manager_wifi_scan(net_ap_info_t *ap_list, int max_count);

esp_err_t net_manager_ethernet_start(void);
esp_err_t net_manager_ethernet_stop(void);

net_info_t *net_manager_get_info(void);
void net_manager_register_callback(void (*cb)(net_info_t *info));

#ifdef __cplusplus
}
#endif
