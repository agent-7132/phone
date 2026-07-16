#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *info_app_create(void);
void info_app_on_exit(void);

#ifdef __cplusplus
}
#endif
