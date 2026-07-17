#include "net_manager.h"
#include "esp_log.h"
#include "esp_eth_driver.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
#include "esp_wifi.h"
#endif

static const char *TAG = "NET_MANAGER";

static net_info_t s_net_info = {0};
static void (*s_callback)(net_info_t *info) = NULL;
static EventGroupHandle_t s_net_event_group = NULL;

#define NET_CONNECTED_BIT BIT0
#define NET_FAIL_BIT BIT1

static esp_netif_t *s_wifi_netif = NULL;
static esp_eth_handle_t s_eth_handle = NULL;

static void update_net_info(net_type_t type, net_state_t state, const char *ip)
{
    s_net_info.type = type;
    s_net_info.state = state;
    if (ip) strncpy(s_net_info.ip, ip, sizeof(s_net_info.ip) - 1);
    
    if (s_callback) {
        s_callback(&s_net_info);
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started");
                break;
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
                strncpy((char *)s_net_info.ssid, (const char *)event->ssid, sizeof(s_net_info.ssid) - 1);
                ESP_LOGI(TAG, "WiFi connected to %s", s_net_info.ssid);
                update_net_info(NET_TYPE_WIFI, NET_STATE_CONNECTING, "");
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected");
                update_net_info(NET_TYPE_WIFI, NET_STATE_DISCONNECTED, "");
                xEventGroupSetBits(s_net_event_group, NET_FAIL_BIT);
                break;
            default:
                break;
        }
    } else
#endif
    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "WiFi got IP: %s", ip_str);
                update_net_info(NET_TYPE_WIFI, NET_STATE_CONNECTED, ip_str);
                xEventGroupSetBits(s_net_event_group, NET_CONNECTED_BIT);
                break;
            }
            case IP_EVENT_ETH_GOT_IP: {
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "Ethernet got IP: %s", ip_str);
                update_net_info(NET_TYPE_ETHERNET, NET_STATE_CONNECTED, ip_str);
                xEventGroupSetBits(s_net_event_group, NET_CONNECTED_BIT);
                break;
            }
            default:
                break;
        }
    } else if (event_base == ETH_EVENT) {
        switch (event_id) {
            case ETHERNET_EVENT_START:
                ESP_LOGI(TAG, "Ethernet started");
                break;
            case ETHERNET_EVENT_STOP:
                ESP_LOGI(TAG, "Ethernet stopped");
                update_net_info(NET_TYPE_ETHERNET, NET_STATE_DISCONNECTED, "");
                break;
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet connected");
                update_net_info(NET_TYPE_ETHERNET, NET_STATE_CONNECTING, "");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "Ethernet disconnected");
                update_net_info(NET_TYPE_ETHERNET, NET_STATE_DISCONNECTED, "");
                break;
            default:
                break;
        }
    }
}

esp_err_t net_manager_init(void)
{
    s_net_event_group = xEventGroupCreate();
    if (!s_net_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    if (!s_wifi_netif) {
        ESP_LOGE(TAG, "Failed to create WiFi netif");
        return ESP_FAIL;
    }

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler");
        return ret;
    }
#else
    ESP_LOGI(TAG, "WiFi not available on ESP32-P4, skipping WiFi init");
#endif

    ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler");
        return ret;
    }

    ret = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Ethernet event handler");
        return ret;
    }

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
    wifi_init_config_t cfg = {
        .static_rx_buf_num = 10,
        .dynamic_rx_buf_num = 32,
        .tx_buf_type = 1,
        .static_tx_buf_num = 0,
        .dynamic_tx_buf_num = 32,
        .rx_mgmt_buf_type = 0,
        .rx_mgmt_buf_num = 5,
        .cache_tx_buf_num = 0,
        .csi_enable = 0,
        .ampdu_rx_enable = 1,
        .ampdu_tx_enable = 1,
        .amsdu_tx_enable = 0,
        .nvs_enable = 1,
        .nano_enable = 0,
        .rx_ba_win = 6,
        .wifi_task_core_id = 0,
        .beacon_max_len = 752,
        .mgmt_sbuf_num = 32,
        .feature_caps = 0,
        .sta_disconnected_pm = 1,
        .espnow_max_encrypt_num = 7,
        .tx_hetb_queue_num = 0,
        .dump_hesigb_enable = 0,
        .magic = 0x1F2F3F4F
    };
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    update_net_info(NET_TYPE_NONE, NET_STATE_DISCONNECTED, "");
    ESP_LOGI(TAG, "Network manager initialized");

    return ESP_OK;
}

esp_err_t net_manager_deinit(void)
{
    esp_err_t ret = ESP_OK;

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(ret));
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_deinit failed: %s", esp_err_to_name(ret));
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler);
#endif
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, event_handler);
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, event_handler);

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
    if (s_wifi_netif) {
        esp_netif_destroy(s_wifi_netif);
        s_wifi_netif = NULL;
    }
#endif

    if (s_net_event_group) {
        vEventGroupDelete(s_net_event_group);
        s_net_event_group = NULL;
    }

    ESP_LOGI(TAG, "Network manager deinitialized");
    return ret;
}

#if CONFIG_ESP_WIFI_REMOTE_ENABLED || CONFIG_ESP_WIFI_ENABLED
esp_err_t net_manager_wifi_connect(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    update_net_info(NET_TYPE_WIFI, NET_STATE_CONNECTING, "");

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
        update_net_info(NET_TYPE_WIFI, NET_STATE_FAILED, "");
        return ret;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
        update_net_info(NET_TYPE_WIFI, NET_STATE_FAILED, "");
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(s_net_event_group,
                                           NET_CONNECTED_BIT | NET_FAIL_BIT,
                                           pdTRUE, pdFALSE, 10000 / portTICK_PERIOD_MS);

    if (bits & NET_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "WiFi connection failed or timeout");
        update_net_info(NET_TYPE_WIFI, NET_STATE_FAILED, "");
        return ESP_FAIL;
    }
}

esp_err_t net_manager_wifi_disconnect(void)
{
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_disconnect failed: %s", esp_err_to_name(ret));
        return ret;
    }
    update_net_info(NET_TYPE_WIFI, NET_STATE_DISCONNECTED, "");
    ESP_LOGI(TAG, "WiFi disconnected");
    return ESP_OK;
}

int net_manager_wifi_scan(net_ap_info_t *ap_list, int max_count)
{
    ESP_LOGI(TAG, "Scanning WiFi networks...");
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(ret));
        return -1;
    }

    uint16_t ap_count = 0;
    ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_scan_get_ap_num failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "Found %d WiFi networks", ap_count);

    if (ap_count == 0) {
        return 0;
    }

    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)heap_caps_malloc(sizeof(wifi_ap_record_t) * ap_count, MALLOC_CAP_SPIRAM);
    if (!ap_info) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP info");
        return -1;
    }

    ret = esp_wifi_scan_get_ap_records(&ap_count, ap_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_scan_get_ap_records failed: %s", esp_err_to_name(ret));
        free(ap_info);
        return -1;
    }

    int result_count = (ap_count > max_count) ? max_count : ap_count;
    for (int i = 0; i < result_count; i++) {
        strncpy(ap_list[i].ssid, (const char *)ap_info[i].ssid, sizeof(ap_list[i].ssid) - 1);
        ap_list[i].rssi = ap_info[i].rssi;
        ap_list[i].channel = ap_info[i].primary;
    }

    free(ap_info);
    return result_count;
}
#else
esp_err_t net_manager_wifi_connect(const char *ssid, const char *password)
{
    ESP_LOGW(TAG, "WiFi not available on ESP32-P4");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_manager_wifi_disconnect(void)
{
    ESP_LOGW(TAG, "WiFi not available on ESP32-P4");
    return ESP_ERR_NOT_SUPPORTED;
}

int net_manager_wifi_scan(net_ap_info_t *ap_list, int max_count)
{
    (void)ap_list;
    (void)max_count;
    ESP_LOGW(TAG, "WiFi not available on ESP32-P4");
    return -1;
}
#endif

esp_err_t net_manager_ethernet_start(void)
{
    if (s_eth_handle) {
        ESP_LOGW(TAG, "Ethernet already started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting Ethernet...");
    update_net_info(NET_TYPE_ETHERNET, NET_STATE_CONNECTING, "");

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    if (!eth_netif) {
        ESP_LOGE(TAG, "Failed to create Ethernet netif");
        update_net_info(NET_TYPE_ETHERNET, NET_STATE_FAILED, "");
        return ESP_FAIL;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create MAC instance");
        esp_netif_destroy(eth_netif);
        update_net_info(NET_TYPE_ETHERNET, NET_STATE_FAILED, "");
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create PHY instance");
        mac->del(mac);
        esp_netif_destroy(eth_netif);
        update_net_info(NET_TYPE_ETHERNET, NET_STATE_FAILED, "");
        return ESP_FAIL;
    }

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t ret = esp_eth_driver_install(&config, &s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(ret));
        mac->del(mac);
        phy->del(phy);
        esp_netif_destroy(eth_netif);
        update_net_info(NET_TYPE_ETHERNET, NET_STATE_FAILED, "");
        return ret;
    }

    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start failed: %s", esp_err_to_name(ret));
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
        mac->del(mac);
        phy->del(phy);
        esp_netif_destroy(eth_netif);
        update_net_info(NET_TYPE_ETHERNET, NET_STATE_FAILED, "");
        return ret;
    }

    ESP_LOGI(TAG, "Ethernet started");
    return ESP_OK;
}

esp_err_t net_manager_ethernet_stop(void)
{
    if (!s_eth_handle) {
        ESP_LOGW(TAG, "Ethernet not started");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping Ethernet...");

    esp_err_t ret = esp_eth_stop(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_stop failed: %s", esp_err_to_name(ret));
    }

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
    esp_eth_get_mac_instance(s_eth_handle, &mac);
    esp_eth_get_phy_instance(s_eth_handle, &phy);

    ret = esp_eth_driver_uninstall(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_driver_uninstall failed: %s", esp_err_to_name(ret));
    }

    if (mac) {
        mac->del(mac);
    }
    if (phy) {
        phy->del(phy);
    }

    s_eth_handle = NULL;
    update_net_info(NET_TYPE_ETHERNET, NET_STATE_DISCONNECTED, "");
    ESP_LOGI(TAG, "Ethernet stopped");
    return ESP_OK;
}

net_info_t *net_manager_get_info(void)
{
    return &s_net_info;
}

void net_manager_register_callback(void (*cb)(net_info_t *info))
{
    s_callback = cb;
}
