#include "bt_manager.h"
#include "gatt_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"
#include "bt_audio_service.h"

static const char *TAG = "BT_MANAGER";

#define BT_DEVICE_MAX 20
#define BT_BOND_MAX 10
#define BT_SCAN_DURATION_MS 5000
#define BT_CONNECTION_TIMEOUT_MS 10000

static bt_state_t s_bt_state = BT_STATE_DISABLED;
static bt_state_cb_t s_state_cb = NULL;
static bt_device_found_cb_t s_device_found_cb = NULL;
static bt_connection_cb_t s_connection_cb = NULL;
static bt_scan_complete_cb_t s_scan_complete_cb = NULL;
static SemaphoreHandle_t s_bt_mutex = NULL;
static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;

static bt_device_t s_scan_devices[BT_DEVICE_MAX];
static int s_scan_device_count = 0;
static bt_bonded_device_t s_bonded_devices[BT_BOND_MAX];
static int s_bonded_count = 0;

static void set_state(bt_state_t state)
{
    if (xSemaphoreTake(s_bt_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    s_bt_state = state;
    xSemaphoreGive(s_bt_mutex);

    if (s_state_cb) {
        s_state_cb(state);
    }
    ESP_LOGI(TAG, "Bluetooth state changed: %d", state);
}

static int bt_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_gap_disc_desc *desc = &event->disc;
        bt_device_t device = {0};

        device.rssi = desc->rssi;
        device.addr_type = desc->addr.type;
        memcpy(device.addr, desc->addr.val, 6);

        int8_t rssi = desc->rssi;
        uint8_t *adv_data = (uint8_t *)desc->data;
        uint8_t data_len = desc->length_data;

        uint8_t *name = NULL;
        uint8_t name_len = 0;
        int i = 0;
        while (i < data_len) {
            uint8_t field_len = adv_data[i];
            if (field_len == 0) break;
            uint8_t field_type = adv_data[i + 1];
            if ((field_type == 0x08 || field_type == 0x09) && field_len > 1) {
                name = &adv_data[i + 2];
                name_len = field_len - 1;
                break;
            }
            i += field_len + 1;
        }

        if (name && name_len > 0) {
            memcpy(device.name, name, MIN(name_len, sizeof(device.name) - 1));
        }

        if (s_device_found_cb) {
            s_device_found_cb(&device);
        }

        if (s_scan_device_count < BT_DEVICE_MAX) {
            s_scan_devices[s_scan_device_count++] = device;
        }

        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE: {
        ESP_LOGI(TAG, "Scan complete, found %d devices", s_scan_device_count);
        set_state(BT_STATE_IDLE);
        if (s_scan_complete_cb) {
            s_scan_complete_cb();
        }
        return 0;
    }

    case BLE_GAP_EVENT_CONNECT: {
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            set_state(BT_STATE_CONNECTED);
            if (s_connection_cb) {
                s_connection_cb(true, NULL);
            }
            ESP_LOGI(TAG, "Connected to device");
        } else {
            set_state(BT_STATE_IDLE);
            if (s_connection_cb) {
                s_connection_cb(false, NULL);
            }
            ESP_LOGE(TAG, "Connection failed: %d", event->connect.status);
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        set_state(BT_STATE_IDLE);
        if (s_connection_cb) {
            s_connection_cb(false, NULL);
        }
        bt_audio_service_on_gap_event(event);
        ESP_LOGI(TAG, "Disconnected: %d", event->disconnect.reason);
        return 0;
    }

    case BLE_GAP_EVENT_ENC_CHANGE: {
        ESP_LOGI(TAG, "Encryption change: %d", event->enc_change.status);
        return 0;
    }

    case BLE_GAP_EVENT_ADV_COMPLETE: {
        ESP_LOGI(TAG, "Advertisement complete");
        return 0;
    }

    case BLE_GAP_EVENT_SUBSCRIBE: {
        gatt_service_on_gap_event(event);
        bt_audio_service_on_gap_event(event);
        return 0;
    }

    case BLE_GAP_EVENT_MTU: {
        ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
        return 0;
    }

    default:
        return 0;
    }
}

static void bt_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void bt_on_sync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer address type: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "Bluetooth synchronized, own address type: %d", s_own_addr_type);
    set_state(BT_STATE_IDLE);
}

static void bt_on_reset(int reason)
{
    ESP_LOGE(TAG, "Bluetooth reset: %d", reason);
}

esp_err_t bt_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth manager (via ESP-Hosted)...");

    s_bt_mutex = xSemaphoreCreateMutex();
    if (!s_bt_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble: %d", rc);
        if (s_bt_mutex) {
            vSemaphoreDelete(s_bt_mutex);
            s_bt_mutex = NULL;
        }
        return ESP_FAIL;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    esp_err_t gatt_ret = gatt_service_init();
    if (gatt_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GATT service: %s", esp_err_to_name(gatt_ret));
        nimble_port_deinit();
        if (s_bt_mutex) {
            vSemaphoreDelete(s_bt_mutex);
            s_bt_mutex = NULL;
        }
        return gatt_ret;
    }

    esp_err_t audio_ret = bt_audio_service_init();
    if (audio_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init audio service: %s", esp_err_to_name(audio_ret));
        nimble_port_deinit();
        if (s_bt_mutex) {
            vSemaphoreDelete(s_bt_mutex);
            s_bt_mutex = NULL;
        }
        return audio_ret;
    }

    ble_hs_cfg.sync_cb = bt_on_sync;
    ble_hs_cfg.reset_cb = bt_on_reset;

    nimble_port_freertos_init(bt_host_task);

    ESP_LOGI(TAG, "Bluetooth manager initialized (NimBLE over ESP-Hosted VHCI)");
    ESP_LOGI(TAG, "ESP32-P4 communicates with ESP32-C6 via SDIO/Wi-Fi Remote");

    return ESP_OK;
}

esp_err_t bt_manager_enable(void)
{
    ESP_LOGI(TAG, "Enabling Bluetooth...");
    
    if (s_bt_state == BT_STATE_IDLE || s_bt_state == BT_STATE_SCANNING || 
        s_bt_state == BT_STATE_CONNECTING || s_bt_state == BT_STATE_CONNECTED) {
        ESP_LOGW(TAG, "Bluetooth is already enabled, current state: %d", s_bt_state);
        return ESP_OK;
    }
    
    ble_hs_sched_start();
    
    set_state(BT_STATE_IDLE);
    ESP_LOGI(TAG, "Bluetooth enabled successfully");
    return ESP_OK;
}

esp_err_t bt_manager_disable(void)
{
    ESP_LOGI(TAG, "Disabling Bluetooth...");
    set_state(BT_STATE_DISABLED);
    return ESP_OK;
}

bt_state_t bt_manager_get_state(void)
{
    bt_state_t state;
    if (xSemaphoreTake(s_bt_mutex, portMAX_DELAY) != pdTRUE) {
        return BT_STATE_DISABLED;
    }
    state = s_bt_state;
    xSemaphoreGive(s_bt_mutex);
    return state;
}

esp_err_t bt_manager_start_scan(void)
{
    ESP_LOGI(TAG, "Starting Bluetooth scan...");

    if (s_bt_state != BT_STATE_IDLE) {
        ESP_LOGE(TAG, "Bluetooth not ready, current state: %d", s_bt_state);
        return ESP_ERR_INVALID_STATE;
    }

    s_scan_device_count = 0;
    set_state(BT_STATE_SCANNING);

    struct ble_gap_disc_params disc_params = {
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 0,
    };

    int rc = ble_gap_disc(s_own_addr_type, BT_SCAN_DURATION_MS, &disc_params, bt_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scan: %d", rc);
        set_state(BT_STATE_IDLE);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bt_manager_stop_scan(void)
{
    ESP_LOGI(TAG, "Stopping Bluetooth scan...");
    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to stop scan: %d", rc);
        return ESP_FAIL;
    }
    set_state(BT_STATE_IDLE);
    return ESP_OK;
}

static int s_connect_attempts = 0;
#define BT_CONNECT_MAX_RETRY 3
#define BT_CONNECT_RETRY_DELAY_MS 2000

static esp_err_t bt_manager_connect_internal(const uint8_t *addr, uint8_t addr_type, int retry_count)
{
    if (!addr) {
        ESP_LOGE(TAG, "Address is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to device (attempt %d/%d)...", retry_count + 1, BT_CONNECT_MAX_RETRY);

    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 0,
        .scan_window = 0,
        .itvl_min = BLE_GAP_CONN_ITVL_MS(100),
        .itvl_max = BLE_GAP_CONN_ITVL_MS(200),
        .latency = 0,
        .supervision_timeout = BLE_GAP_SUPERVISION_TIMEOUT_MS(2000),
    };

    ble_addr_t peer_addr;
    peer_addr.type = addr_type;
    memcpy(peer_addr.val, addr, 6);

    int rc = ble_gap_connect(s_own_addr_type, &peer_addr, BT_SCAN_DURATION_MS, &conn_params, bt_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to connect: %d", rc);
        if (retry_count < BT_CONNECT_MAX_RETRY - 1) {
            ESP_LOGI(TAG, "Retrying connection in %d ms...", BT_CONNECT_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(BT_CONNECT_RETRY_DELAY_MS));
            return bt_manager_connect_internal(addr, addr_type, retry_count + 1);
        }
        ESP_LOGE(TAG, "Connection failed after %d attempts", BT_CONNECT_MAX_RETRY);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bt_manager_connect(const uint8_t *addr, uint8_t addr_type)
{
    if (!addr) {
        ESP_LOGE(TAG, "Address is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to device...");

    if (s_bt_state != BT_STATE_IDLE) {
        ESP_LOGE(TAG, "Bluetooth not ready, current state: %d", s_bt_state);
        return ESP_ERR_INVALID_STATE;
    }

    set_state(BT_STATE_CONNECTING);
    s_connect_attempts = 0;

    esp_err_t ret = bt_manager_connect_internal(addr, addr_type, 0);
    if (ret != ESP_OK) {
        set_state(BT_STATE_IDLE);
    }

    return ret;
}

esp_err_t bt_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Disconnecting Bluetooth...");

    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "No active connection");
        return ESP_OK;
    }

    set_state(BT_STATE_DISCONNECTING);

    int rc = ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to disconnect: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t bt_manager_remove_bond(const uint8_t *addr)
{
    if (!addr) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_addr_t peer_addr;
    peer_addr.type = BLE_ADDR_PUBLIC;
    memcpy(peer_addr.val, addr, 6);

    int rc = ble_store_util_delete_peer(&peer_addr);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to remove bond: %d", rc);
        return ESP_FAIL;
    }

    for (int i = 0; i < s_bonded_count; i++) {
        if (memcmp(s_bonded_devices[i].addr, addr, 6) == 0) {
            for (int j = i; j < s_bonded_count - 1; j++) {
                s_bonded_devices[j] = s_bonded_devices[j + 1];
            }
            s_bonded_count--;
            break;
        }
    }

    ESP_LOGI(TAG, "Bond removed");
    return ESP_OK;
}

int bt_manager_get_bonded_devices(bt_bonded_device_t *devices, int max_count)
{
    if (!devices || max_count <= 0) {
        return 0;
    }

    int count = MIN(s_bonded_count, max_count);
    memcpy(devices, s_bonded_devices, count * sizeof(bt_bonded_device_t));
    return count;
}

void bt_manager_register_state_cb(bt_state_cb_t cb)
{
    s_state_cb = cb;
}

void bt_manager_register_device_found_cb(bt_device_found_cb_t cb)
{
    s_device_found_cb = cb;
}

void bt_manager_register_connection_cb(bt_connection_cb_t cb)
{
    s_connection_cb = cb;
}

void bt_manager_register_scan_complete_cb(bt_scan_complete_cb_t cb)
{
    s_scan_complete_cb = cb;
}

