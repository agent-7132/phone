#pragma once

#include "esp_err.h"
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_AUDIO_EVENT_CONNECTED,
    BT_AUDIO_EVENT_DISCONNECTED,
    BT_AUDIO_EVENT_STREAM_START,
    BT_AUDIO_EVENT_STREAM_STOP,
    BT_AUDIO_EVENT_CONFIG_CHANGED,
} bt_audio_event_t;

typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t byte_rate;
} audio_config_t;

typedef void (*bt_audio_status_cb)(bt_audio_event_t event, audio_config_t *config, void *arg);
typedef void (*bt_audio_data_cb)(const uint8_t *data, size_t len, void *arg);

esp_err_t bt_audio_service_init(void);
esp_err_t bt_audio_service_deinit(void);

void bt_audio_service_start_stream(void);
void bt_audio_service_stop_stream(void);

bool bt_audio_service_is_streaming(void);

void bt_audio_service_send_data(const uint8_t *data, size_t len);

void bt_audio_service_set_config(audio_config_t *config);
void bt_audio_service_get_config(audio_config_t *config);

void bt_audio_service_register_callback(bt_audio_status_cb cb, void *arg);
void bt_audio_service_register_data_callback(bt_audio_data_cb cb, void *arg);

void bt_audio_service_on_gap_event(struct ble_gap_event *event);

#ifdef __cplusplus
}
#endif