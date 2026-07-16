#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    size_t len;
    int width;
    int height;
    int format;
} bsp_camera_frame_t;

esp_err_t bsp_camera_init(void);
esp_err_t bsp_camera_deinit(void);
bsp_camera_frame_t *bsp_camera_capture(void);
void bsp_camera_return_frame(bsp_camera_frame_t *frame);
bool bsp_camera_is_initialized(void);
esp_err_t bsp_camera_start_preview(void);
esp_err_t bsp_camera_stop_preview(void);

#ifdef __cplusplus
}
#endif