#pragma once
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *control_center;
    lv_obj_t *wifi_btn;
    lv_obj_t *bt_btn;
    lv_obj_t *flashlight_btn;
    lv_obj_t *airplane_btn;
    lv_obj_t *volume_slider;
    lv_obj_t *brightness_slider;
    lv_obj_t *wifi_label;
    lv_obj_t *bt_label;
    lv_obj_t *flashlight_label;
    lv_obj_t *airplane_label;
    lv_obj_t *wifi_status_label;
    lv_obj_t *bt_status_label;
    lv_obj_t *volume_value_label;
    lv_obj_t *brightness_value_label;
    bool wifi_enabled;
    bool bt_enabled;
    bool flashlight_enabled;
    bool airplane_mode;
} control_center_t;

esp_err_t control_center_init(void);
void control_center_show(lv_obj_t *parent);
void control_center_hide(void);
lv_obj_t *control_center_get(void);

void control_center_update_wifi_status(bool enabled, bool connected, const char *ssid);
void control_center_update_bt_status(bool enabled, bool connected, const char *device_name);

#ifdef __cplusplus
}
#endif