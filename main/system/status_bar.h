#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *clock_label;
    lv_obj_t *memory_label;
    lv_obj_t *sensor_label;
    lv_obj_t *net_icon;
    lv_obj_t *bt_icon;
    lv_obj_t *notification_icon;
} status_bar_t;

status_bar_t *status_bar_create(lv_obj_t *parent);
void status_bar_update_clock(void);
void status_bar_update_memory(void);
void status_bar_set_sensor_data(float temperature, float humidity);
void status_bar_set_network(int type, bool connected);
void status_bar_set_bluetooth(bool enabled, bool connected);
void status_bar_set_notification(int count);

#ifdef __cplusplus
}
#endif
