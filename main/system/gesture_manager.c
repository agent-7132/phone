#include "gesture_manager.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "GESTURE_MANAGER";

#define GESTURE_THRESHOLD 50
#define GESTURE_LONG_PRESS_TIME 500
#define GESTURE_DOUBLE_TAP_TIME 300

static gesture_callback_t s_callback = NULL;
static bool s_enabled = true;
static lv_point_t s_start_point = {0, 0};
static lv_point_t s_end_point = {0, 0};
static uint32_t s_press_time = 0;
static uint32_t s_last_tap_time = 0;
static bool s_is_pressed = false;

static void handle_swipe(lv_point_t start, lv_point_t end);
static void gesture_event_cb(lv_event_t *e);

esp_err_t gesture_manager_init(void)
{
    s_callback = NULL;
    s_enabled = true;
    s_start_point = (lv_point_t){0, 0};
    s_end_point = (lv_point_t){0, 0};
    s_press_time = 0;
    s_last_tap_time = 0;
    s_is_pressed = false;

    lv_obj_add_event_cb(lv_screen_active(), gesture_event_cb, LV_EVENT_ALL, NULL);

    ESP_LOGI(TAG, "Gesture manager initialized");
    return ESP_OK;
}

void gesture_manager_register_callback(gesture_callback_t cb)
{
    s_callback = cb;
}

void gesture_manager_unregister_callback(gesture_callback_t cb)
{
    if (s_callback == cb) {
        s_callback = NULL;
    }
}

void gesture_manager_set_enabled(bool enabled)
{
    s_enabled = enabled;
}

bool gesture_manager_is_enabled(void)
{
    return s_enabled;
}

static void handle_swipe(lv_point_t start, lv_point_t end)
{
    if (!s_callback || !s_enabled) return;

    int dx = end.x - start.x;
    int dy = end.y - start.y;
    int abs_dx = dx > 0 ? dx : -dx;
    int abs_dy = dy > 0 ? dy : -dy;

    if (abs_dx > abs_dy && abs_dx > GESTURE_THRESHOLD) {
        if (dx > 0) {
            s_callback(GESTURE_TYPE_SWIPE_RIGHT, start, end);
        } else {
            s_callback(GESTURE_TYPE_SWIPE_LEFT, start, end);
        }
    } else if (abs_dy > abs_dx && abs_dy > GESTURE_THRESHOLD) {
        if (dy > 0) {
            s_callback(GESTURE_TYPE_SWIPE_DOWN, start, end);
        } else {
            s_callback(GESTURE_TYPE_SWIPE_UP, start, end);
        }
    }
}

static void gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();

    if (!s_enabled) return;

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_start_point);
        s_press_time = lv_tick_get();
        s_is_pressed = true;
    } else if (code == LV_EVENT_RELEASED) {
        if (!s_is_pressed) return;

        lv_indev_get_point(indev, &s_end_point);
        uint32_t release_time = lv_tick_get();
        uint32_t press_duration = release_time - s_press_time;

        if (press_duration >= GESTURE_LONG_PRESS_TIME) {
            if (s_callback) {
                s_callback(GESTURE_TYPE_LONG_PRESS, s_start_point, s_end_point);
            }
        } else {
            int dx = s_end_point.x - s_start_point.x;
            int dy = s_end_point.y - s_start_point.y;
            int abs_dx = dx > 0 ? dx : -dx;
            int abs_dy = dy > 0 ? dy : -dy;

            if (abs_dx < GESTURE_THRESHOLD && abs_dy < GESTURE_THRESHOLD) {
                uint32_t now = lv_tick_get();
                if (now - s_last_tap_time < GESTURE_DOUBLE_TAP_TIME) {
                    if (s_callback) {
                        s_callback(GESTURE_TYPE_DOUBLE_TAP, s_start_point, s_end_point);
                    }
                } else {
                    if (s_callback) {
                        s_callback(GESTURE_TYPE_TAP, s_start_point, s_end_point);
                    }
                }
                s_last_tap_time = now;
            } else {
                handle_swipe(s_start_point, s_end_point);
            }
        }

        s_is_pressed = false;
    }
}