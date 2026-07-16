#include "bt_audio_service.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gatt/ble_svc_gatt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "host/ble_hs_mbuf.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "BT_AUDIO_SERVICE";

#define AUDIO_SERVICE_UUID 0xFFE0
#define AUDIO_DATA_CHR_UUID 0xFFE1
#define AUDIO_CONFIG_CHR_UUID 0xFFE2
#define AUDIO_STATUS_CHR_UUID 0xFFE3

#define MAX_AUDIO_PACKET_SIZE 120
#define AUDIO_QUEUE_SIZE 64
#define MAX_BUFFER_SIZE 32768

typedef struct {
    uint8_t data[MAX_AUDIO_PACKET_SIZE];
    size_t len;
} audio_packet_t;

static uint16_t audio_data_handle;
static uint16_t audio_config_handle;
static uint16_t audio_status_handle;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool is_streaming = false;
static QueueHandle_t audio_queue = NULL;
static audio_config_t current_config = {16000, 1, 16, 32000};
static uint8_t *audio_buffer = NULL;
static size_t buffer_len = 0;
static bt_audio_status_cb status_callback = NULL;
static bt_audio_data_cb data_callback = NULL;
static void *callback_arg = NULL;

static int audio_data_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int audio_config_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int audio_status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);

static ble_uuid16_t audio_service_uuid = BLE_UUID16_INIT(AUDIO_SERVICE_UUID);
static ble_uuid16_t audio_data_uuid = BLE_UUID16_INIT(AUDIO_DATA_CHR_UUID);
static ble_uuid16_t audio_config_uuid = BLE_UUID16_INIT(AUDIO_CONFIG_CHR_UUID);
static ble_uuid16_t audio_status_uuid = BLE_UUID16_INIT(AUDIO_STATUS_CHR_UUID);

static const struct ble_gatt_chr_def audio_chrs[] = {
    {
        .uuid = &audio_data_uuid.u,
        .access_cb = audio_data_access_cb,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &audio_data_handle,
    },
    {
        .uuid = &audio_config_uuid.u,
        .access_cb = audio_config_access_cb,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .val_handle = &audio_config_handle,
    },
    {
        .uuid = &audio_status_uuid.u,
        .access_cb = audio_status_access_cb,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &audio_status_handle,
    },
    {
        0,
    }
};

static const struct ble_gatt_svc_def audio_svc = {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &audio_service_uuid.u,
    .characteristics = audio_chrs,
};

static int audio_data_access_cb(uint16_t ch_conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (attr_handle == audio_data_handle) {
            if (buffer_len > 0) {
                size_t send_len = (buffer_len > MAX_AUDIO_PACKET_SIZE) ? MAX_AUDIO_PACKET_SIZE : buffer_len;
                os_mbuf_append(ctxt->om, audio_buffer, send_len);
                memmove(audio_buffer, audio_buffer + send_len, buffer_len - send_len);
                buffer_len -= send_len;
            } else {
                uint8_t dummy_data[1] = {0};
                os_mbuf_append(ctxt->om, dummy_data, sizeof(dummy_data));
            }
        }
        return 0;
    }

    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static int audio_config_access_cb(uint16_t ch_conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t config_data[12];
        memcpy(config_data, &current_config.sample_rate, 4);
        memcpy(config_data + 4, &current_config.num_channels, 2);
        memcpy(config_data + 6, &current_config.bits_per_sample, 2);
        memcpy(config_data + 8, &current_config.byte_rate, 4);
        os_mbuf_append(ctxt->om, config_data, sizeof(config_data));
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (os_mbuf_len(ctxt->om) >= 12) {
            os_mbuf_copydata(ctxt->om, 0, 4, &current_config.sample_rate);
            os_mbuf_copydata(ctxt->om, 4, 2, &current_config.num_channels);
            os_mbuf_copydata(ctxt->om, 6, 2, &current_config.bits_per_sample);
            os_mbuf_copydata(ctxt->om, 8, 4, &current_config.byte_rate);
            
            ESP_LOGI(TAG, "Audio config updated: SR=%d, CH=%d, BP=%d, BR=%d",
                     current_config.sample_rate, current_config.num_channels,
                     current_config.bits_per_sample, current_config.byte_rate);
            
            if (status_callback) {
                status_callback(BT_AUDIO_EVENT_CONFIG_CHANGED, &current_config, callback_arg);
            }
        }
        return 0;
    }

    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static int audio_status_access_cb(uint16_t ch_conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t status = is_streaming ? 1 : 0;
        os_mbuf_append(ctxt->om, &status, sizeof(status));
        return 0;
    }

    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

static void notify_status(void)
{
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        uint8_t status = is_streaming ? 1 : 0;
        struct os_mbuf *txom = os_mbuf_get_pkthdr(0, 0);
        if (txom) {
            os_mbuf_append(txom, &status, sizeof(status));
            ble_gatts_notify_custom(conn_handle, audio_status_handle, txom);
        }
    }
}

void bt_audio_service_on_gap_event(struct ble_gap_event *event)
{
    if (event->type == BLE_GAP_EVENT_SUBSCRIBE) {
        if (event->subscribe.attr_handle == audio_data_handle) {
            if (event->subscribe.cur_notify) {
                is_streaming = true;
                conn_handle = event->subscribe.conn_handle;
                ESP_LOGI(TAG, "Audio streaming enabled via subscribe (conn: %d)", conn_handle);
                
                if (status_callback) {
                    status_callback(BT_AUDIO_EVENT_STREAM_START, NULL, callback_arg);
                }
                
                notify_status();
            } else {
                is_streaming = false;
                conn_handle = BLE_HS_CONN_HANDLE_NONE;
                ESP_LOGI(TAG, "Audio streaming disabled via unsubscribe");
                
                if (status_callback) {
                    status_callback(BT_AUDIO_EVENT_STREAM_STOP, NULL, callback_arg);
                }
                
                notify_status();
            }
        }
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        is_streaming = false;
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        buffer_len = 0;
        ESP_LOGI(TAG, "Audio streaming disabled due to disconnection");
        
        if (status_callback) {
            status_callback(BT_AUDIO_EVENT_DISCONNECTED, NULL, callback_arg);
        }
        
        notify_status();
    } else if (event->type == BLE_GAP_EVENT_CONNECT) {
        ESP_LOGI(TAG, "Bluetooth connected (conn: %d)", event->connect.conn_handle);
        
        if (status_callback) {
            status_callback(BT_AUDIO_EVENT_CONNECTED, NULL, callback_arg);
        }
    }
}

static void audio_stream_task(void *arg)
{
    (void)arg;
    audio_packet_t packet;

    while (1) {
        if (xQueueReceive(audio_queue, &packet, portMAX_DELAY) == pdTRUE) {
            if (packet.len > 0 && is_streaming && conn_handle != BLE_HS_CONN_HANDLE_NONE) {
                struct os_mbuf *txom = os_mbuf_get_pkthdr(0, 0);
                if (!txom) {
                    ESP_LOGE(TAG, "Failed to allocate mbuf");
                    continue;
                }
                os_mbuf_append(txom, packet.data, packet.len);

                int rc = ble_gatts_notify_custom(conn_handle, audio_data_handle, txom);
                if (rc != 0) {
                    ESP_LOGW(TAG, "Failed to notify audio data: %d", rc);
                    os_mbuf_free(txom);
                }
            }
        }
    }
}

esp_err_t bt_audio_service_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth Audio Service...");

    audio_queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(audio_packet_t));
    if (!audio_queue) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return ESP_ERR_NO_MEM;
    }

    audio_buffer = heap_caps_malloc(MAX_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer in PSRAM");
        vQueueDelete(audio_queue);
        audio_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    memset(audio_buffer, 0, MAX_BUFFER_SIZE);
    buffer_len = 0;

    int rc = ble_gatts_count_cfg(&audio_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT service: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(&audio_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT service: %d", rc);
        return ESP_FAIL;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(audio_stream_task, "bt_audio_stream", 2048, NULL, 12, NULL, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio stream task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Bluetooth Audio Service initialized");
    return ESP_OK;
}

void bt_audio_service_start_stream(void)
{
    is_streaming = true;
    ESP_LOGI(TAG, "Audio stream started");
    notify_status();
    
    if (status_callback) {
        status_callback(BT_AUDIO_EVENT_STREAM_START, NULL, callback_arg);
    }
}

void bt_audio_service_stop_stream(void)
{
    is_streaming = false;
    buffer_len = 0;
    ESP_LOGI(TAG, "Audio stream stopped");
    notify_status();
    
    if (status_callback) {
        status_callback(BT_AUDIO_EVENT_STREAM_STOP, NULL, callback_arg);
    }
}

bool bt_audio_service_is_streaming(void)
{
    return is_streaming;
}

void bt_audio_service_send_data(const uint8_t *data, size_t len)
{
    if (!is_streaming || !data || len == 0) {
        return;
    }

    size_t to_send = len;
    size_t offset = 0;

    while (to_send > 0) {
        audio_packet_t packet;
        packet.len = (to_send > MAX_AUDIO_PACKET_SIZE) ? MAX_AUDIO_PACKET_SIZE : to_send;
        memcpy(packet.data, data + offset, packet.len);
        
        if (xQueueSend(audio_queue, &packet, 0) != pdPASS) {
            ESP_LOGW(TAG, "Audio queue full, dropping data");
        }
        
        offset += packet.len;
        to_send -= packet.len;
    }
}

void bt_audio_service_set_config(audio_config_t *config)
{
    if (config) {
        current_config = *config;
        ESP_LOGI(TAG, "Audio config set: SR=%d, CH=%d, BP=%d",
                 config->sample_rate, config->num_channels, config->bits_per_sample);
    }
}

void bt_audio_service_get_config(audio_config_t *config)
{
    if (config) {
        *config = current_config;
    }
}

void bt_audio_service_register_callback(bt_audio_status_cb cb, void *arg)
{
    status_callback = cb;
    callback_arg = arg;
    ESP_LOGI(TAG, "Status callback registered");
}

void bt_audio_service_register_data_callback(bt_audio_data_cb cb, void *arg)
{
    data_callback = cb;
    callback_arg = arg;
    ESP_LOGI(TAG, "Data callback registered");
}

esp_err_t bt_audio_service_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Bluetooth Audio Service...");
    
    bt_audio_service_stop_stream();
    
    if (audio_queue) {
        vQueueDelete(audio_queue);
        audio_queue = NULL;
    }
    
    if (audio_buffer) {
        heap_caps_free(audio_buffer);
        audio_buffer = NULL;
    }
    
    buffer_len = 0;
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    is_streaming = false;
    status_callback = NULL;
    data_callback = NULL;
    callback_arg = NULL;
    
    ESP_LOGI(TAG, "Bluetooth Audio Service deinitialized");
    return ESP_OK;
}