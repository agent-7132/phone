#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *encrypt_app_create(void);
void encrypt_app_on_exit(void);

#ifdef __cplusplus
}
#endif
