#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *clock_label;
    lv_obj_t *memory_label;
    lv_obj_t *wifi_icon;
    lv_obj_t *battery_icon;
} status_bar_t;

status_bar_t *status_bar_create(lv_obj_t *parent);
void status_bar_update_clock(void);
void status_bar_update_memory(void);
void status_bar_set_wifi(bool connected);
void status_bar_set_battery(int percent);

#ifdef __cplusplus
}
#endif
