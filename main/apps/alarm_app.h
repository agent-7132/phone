#pragma once

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *alarm_app_create(void);
void alarm_app_on_exit(void);

#ifdef __cplusplus
}
#endif