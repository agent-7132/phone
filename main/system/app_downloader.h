#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DOWNLOAD_STATE_IDLE,
    DOWNLOAD_STATE_DOWNLOADING,
    DOWNLOAD_STATE_INSTALLING,
    DOWNLOAD_STATE_COMPLETED,
    DOWNLOAD_STATE_FAILED,
} download_state_t;

typedef void (*download_progress_cb_t)(int progress, download_state_t state);
typedef void (*download_complete_cb_t)(bool success, const char *app_name);

void app_downloader_init(void);

esp_err_t app_downloader_start(const char *url, const char *app_name,
                               download_progress_cb_t progress_cb,
                               download_complete_cb_t complete_cb);

download_state_t app_downloader_get_state(void);
int app_downloader_get_progress(void);
void app_downloader_cancel(void);

#ifdef __cplusplus
}
#endif