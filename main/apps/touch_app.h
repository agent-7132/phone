#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *touch_app_create(void);
void touch_app_on_exit(void);

#ifdef __cplusplus
}
#endif
