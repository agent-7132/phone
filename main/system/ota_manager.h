#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_UPDATING,
    OTA_STATE_COMPLETED,
    OTA_STATE_FAILED,
    OTA_STATE_PENDING_VERIFY
} ota_state_t;

typedef void (*ota_progress_cb_t)(int progress, ota_state_t state);
typedef void (*ota_complete_cb_t)(bool success);

esp_err_t ota_manager_init(void);
esp_err_t ota_manager_start_update(const char *firmware_url);
ota_state_t ota_manager_get_state(void);
int ota_manager_get_progress(void);
void ota_manager_register_progress_cb(ota_progress_cb_t cb);
void ota_manager_register_complete_cb(ota_complete_cb_t cb);
void ota_manager_reboot(void);

esp_err_t ota_manager_mark_app_valid(void);
esp_err_t ota_manager_mark_app_invalid_and_reboot(void);
bool ota_manager_has_pending_update(void);

#ifdef __cplusplus
}
#endif