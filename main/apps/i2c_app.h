#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *i2c_app_create(void);
void i2c_app_on_exit(void);

#ifdef __cplusplus
}
#endif
