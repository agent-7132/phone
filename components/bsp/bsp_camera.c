#include "bsp/bsp_camera.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr.h"

static const char *TAG = "bsp_camera";

static bool s_is_initialized = false;
static esp_cam_ctlr_handle_t s_ctlr_handle = NULL;

#define BSP_CAMERA_WIDTH      480
#define BSP_CAMERA_HEIGHT     800
#define BSP_CAMERA_LANE_RATE  200

esp_err_t bsp_camera_init(void)
{
    esp_err_t ret;

    if (s_is_initialized) {
        return ESP_OK;
    }

    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = 0,
        .h_res = BSP_CAMERA_WIDTH,
        .v_res = BSP_CAMERA_HEIGHT,
        .lane_bit_rate_mbps = BSP_CAMERA_LANE_RATE,
        .input_data_color_type = CAM_CTLR_COLOR_RGB565,
        .output_data_color_type = CAM_CTLR_COLOR_RGB565,
        .data_lane_num = 2,
        .byte_swap_en = false,
        .queue_items = 2,
    };

    ESP_LOGI(TAG, "Initializing MIPI-CSI camera (w=%d, h=%d, rate=%d Mbps)",
             BSP_CAMERA_WIDTH, BSP_CAMERA_HEIGHT, BSP_CAMERA_LANE_RATE);

    ESP_GOTO_ON_ERROR(esp_cam_new_csi_ctlr(&csi_config, &s_ctlr_handle), failed, TAG, "failed to create CSI controller");

    ESP_GOTO_ON_ERROR(esp_cam_ctlr_enable(s_ctlr_handle), failed, TAG, "failed to enable controller");

    ESP_GOTO_ON_ERROR(esp_cam_ctlr_start(s_ctlr_handle), failed, TAG, "failed to start controller");

    s_is_initialized = true;
    ESP_LOGI(TAG, "MIPI-CSI camera initialized successfully");

    return ESP_OK;

failed:
    if (s_ctlr_handle) {
        esp_cam_ctlr_del(s_ctlr_handle);
        s_ctlr_handle = NULL;
    }
    return ret;
}

esp_err_t bsp_camera_deinit(void)
{
    if (!s_is_initialized) {
        return ESP_OK;
    }

    if (s_ctlr_handle) {
        esp_cam_ctlr_stop(s_ctlr_handle);
        esp_cam_ctlr_disable(s_ctlr_handle);
        esp_cam_ctlr_del(s_ctlr_handle);
        s_ctlr_handle = NULL;
    }

    s_is_initialized = false;
    ESP_LOGI(TAG, "MIPI-CSI camera deinitialized");

    return ESP_OK;
}

bsp_camera_frame_t *bsp_camera_capture(void)
{
    if (!s_is_initialized || !s_ctlr_handle) {
        return NULL;
    }

    static bsp_camera_frame_t frame = {0};
    const void *fb = NULL;
    size_t fb_len = 0;

    esp_err_t ret = esp_cam_ctlr_get_frame_buffer(s_ctlr_handle, 1, &fb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame buffer: %d", ret);
        return NULL;
    }

    ret = esp_cam_ctlr_get_frame_buffer_len(s_ctlr_handle, &fb_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame buffer length: %d", ret);
        return NULL;
    }

    frame.buf = (uint8_t *)fb;
    frame.len = fb_len;
    frame.width = BSP_CAMERA_WIDTH;
    frame.height = BSP_CAMERA_HEIGHT;
    frame.format = CAM_CTLR_COLOR_RGB565;

    return &frame;
}

void bsp_camera_return_frame(bsp_camera_frame_t *frame)
{
    (void)frame;
}

bool bsp_camera_is_initialized(void)
{
    return s_is_initialized;
}

esp_err_t bsp_camera_start_preview(void)
{
    if (!s_is_initialized || !s_ctlr_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_cam_ctlr_start(s_ctlr_handle);
}

esp_err_t bsp_camera_stop_preview(void)
{
    if (!s_is_initialized || !s_ctlr_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_cam_ctlr_stop(s_ctlr_handle);
}