#pragma once
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GESTURE_TYPE_NONE,
    GESTURE_TYPE_SWIPE_LEFT,
    GESTURE_TYPE_SWIPE_RIGHT,
    GESTURE_TYPE_SWIPE_UP,
    GESTURE_TYPE_SWIPE_DOWN,
    GESTURE_TYPE_TAP,
    GESTURE_TYPE_DOUBLE_TAP,
    GESTURE_TYPE_LONG_PRESS,
    GESTURE_TYPE_PINCH_IN,
    GESTURE_TYPE_PINCH_OUT,
    GESTURE_TYPE_LETTER_A,
    GESTURE_TYPE_LETTER_B,
    GESTURE_TYPE_LETTER_C,
    GESTURE_TYPE_LETTER_V,
    GESTURE_TYPE_LETTER_Z,
    GESTURE_TYPE_UNKNOWN
} gesture_type_t;

typedef void (*gesture_callback_t)(gesture_type_t type, lv_point_t start, lv_point_t end);

esp_err_t gesture_manager_init(void);
void gesture_manager_register_callback(gesture_callback_t cb);
void gesture_manager_unregister_callback(gesture_callback_t cb);
void gesture_manager_set_enabled(bool enabled);
bool gesture_manager_is_enabled(void);

#ifdef __cplusplus
}
#endif
