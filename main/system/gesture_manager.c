#include "gesture_manager.h"
#include "esp_log.h"
#include "power_manager.h"
#include <math.h>

static const char *TAG = "GESTURE_MANAGER";

#define GESTURE_THRESHOLD 50
#define GESTURE_LONG_PRESS_TIME 500
#define GESTURE_DOUBLE_TAP_TIME 300
#define MAX_GESTURE_POINTS 64
#define GESTURE_VECTOR_THRESHOLD 0.8f
#define GESTURE_SAMPLE_INTERVAL 10

typedef struct {
    lv_point_t points[MAX_GESTURE_POINTS];
    int count;
    uint32_t start_time;
} gesture_trace_t;

static gesture_callback_t s_callback = NULL;
static bool s_enabled = true;
static lv_point_t s_start_point = {0, 0};
static lv_point_t s_end_point = {0, 0};
static uint32_t s_press_time = 0;
static uint32_t s_last_tap_time = 0;
static bool s_is_pressed = false;

static gesture_trace_t s_gesture_trace = {0};
static bool s_is_drawing = false;

typedef enum {
    GESTURE_LETTER_A,
    GESTURE_LETTER_B,
    GESTURE_LETTER_C,
    GESTURE_LETTER_V,
    GESTURE_LETTER_Z,
    GESTURE_LETTER_UNKNOWN
} gesture_letter_t;

static void handle_swipe(lv_point_t start, lv_point_t end);
static void gesture_event_cb(lv_event_t *e);
static void clear_trace(void);
static void add_point(lv_point_t pt);
static float vector_dot_product(lv_point_t v1, lv_point_t v2);
static float vector_magnitude(lv_point_t v);
static float vector_angle(lv_point_t v1, lv_point_t v2);
static gesture_letter_t recognize_letter(void);

esp_err_t gesture_manager_init(void)
{
    s_callback = NULL;
    s_enabled = true;
    s_start_point = (lv_point_t){0, 0};
    s_end_point = (lv_point_t){0, 0};
    s_press_time = 0;
    s_last_tap_time = 0;
    s_is_pressed = false;
    s_is_drawing = false;
    clear_trace();

    lv_obj_add_event_cb(lv_screen_active(), gesture_event_cb, LV_EVENT_ALL, NULL);

    ESP_LOGI(TAG, "Gesture manager initialized with vector-based recognition");
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

static void clear_trace(void)
{
    s_gesture_trace.count = 0;
    s_gesture_trace.start_time = 0;
}

static void add_point(lv_point_t pt)
{
    if (s_gesture_trace.count >= MAX_GESTURE_POINTS) {
        return;
    }
    s_gesture_trace.points[s_gesture_trace.count++] = pt;
}

static float vector_dot_product(lv_point_t v1, lv_point_t v2)
{
    return (float)(v1.x * v2.x + v1.y * v2.y);
}

static float vector_magnitude(lv_point_t v)
{
    return sqrtf((float)(v.x * v.x + v.y * v.y));
}

static float vector_angle(lv_point_t v1, lv_point_t v2)
{
    float dot = vector_dot_product(v1, v2);
    float mag1 = vector_magnitude(v1);
    float mag2 = vector_magnitude(v2);
    
    if (mag1 < 0.001f || mag2 < 0.001f) {
        return 0.0f;
    }
    
    float cos_theta = dot / (mag1 * mag2);
    cos_theta = cos_theta > 1.0f ? 1.0f : (cos_theta < -1.0f ? -1.0f : cos_theta);
    return acosf(cos_theta);
}

static float vector_similarity(lv_point_t v1, lv_point_t v2)
{
    float dot = vector_dot_product(v1, v2);
    float mag1 = vector_magnitude(v1);
    float mag2 = vector_magnitude(v2);
    
    if (mag1 < 0.001f || mag2 < 0.001f) {
        return 0.0f;
    }
    
    return dot / (mag1 * mag2);
}

static int get_direction(lv_point_t v)
{
    float mag = vector_magnitude(v);
    if (mag < 5.0f) return -1;
    
    float angle = atan2f((float)v.y, (float)v.x);
    float degrees = angle * 180.0f / 3.1415926535f;
    
    if (degrees < 0) degrees += 360;
    
    if (degrees >= 337.5 || degrees < 22.5) return 0;
    if (degrees >= 22.5 && degrees < 67.5) return 1;
    if (degrees >= 67.5 && degrees < 112.5) return 2;
    if (degrees >= 112.5 && degrees < 157.5) return 3;
    if (degrees >= 157.5 && degrees < 202.5) return 4;
    if (degrees >= 202.5 && degrees < 247.5) return 5;
    if (degrees >= 247.5 && degrees < 292.5) return 6;
    return 7;
}

static gesture_letter_t recognize_letter(void)
{
    if (s_gesture_trace.count < 10) {
        return GESTURE_LETTER_UNKNOWN;
    }

    int directions[MAX_GESTURE_POINTS];
    int dir_count = 0;
    int prev_dir = -1;

    for (int i = 1; i < s_gesture_trace.count; i++) {
        lv_point_t v = {
            s_gesture_trace.points[i].x - s_gesture_trace.points[i-1].x,
            s_gesture_trace.points[i].y - s_gesture_trace.points[i-1].y
        };
        
        int dir = get_direction(v);
        if (dir >= 0 && dir != prev_dir) {
            directions[dir_count++] = dir;
            prev_dir = dir;
        }
    }

    if (dir_count < 3) {
        return GESTURE_LETTER_UNKNOWN;
    }

    int start_x = s_gesture_trace.points[0].x;
    int start_y = s_gesture_trace.points[0].y;
    int end_x = s_gesture_trace.points[s_gesture_trace.count-1].x;
    int end_y = s_gesture_trace.points[s_gesture_trace.count-1].y;

    bool is_A = false, is_V = false, is_Z = false, is_C = false;

    if (dir_count >= 3) {
        if (directions[0] == 1 || directions[0] == 0) {
            if ((directions[1] == 3 || directions[1] == 2) && 
                (directions[2] == 5 || directions[2] == 4)) {
                is_A = true;
            }
        }
        
        if ((directions[0] == 1 || directions[0] == 0) && 
            (directions[1] == 3 || directions[1] == 2) && 
            (directions[2] == 5 || directions[2] == 6)) {
            is_V = true;
        }
        
        if ((directions[0] == 1 || directions[0] == 0) && 
            (directions[1] == 2 || directions[1] == 6) && 
            (directions[2] == 5 || directions[2] == 4)) {
            is_Z = true;
        }
    }

    int min_x = 480, max_x = 0, min_y = 800, max_y = 0;
    for (int i = 0; i < s_gesture_trace.count; i++) {
        if (s_gesture_trace.points[i].x < min_x) min_x = s_gesture_trace.points[i].x;
        if (s_gesture_trace.points[i].x > max_x) max_x = s_gesture_trace.points[i].x;
        if (s_gesture_trace.points[i].y < min_y) min_y = s_gesture_trace.points[i].y;
        if (s_gesture_trace.points[i].y > max_y) max_y = s_gesture_trace.points[i].y;
    }

    int width = max_x - min_x;
    int height = max_y - min_y;

    if (width > 20 && height > 20) {
        float aspect = (float)width / (float)height;
        if (aspect > 1.5f && dir_count >= 2) {
            if (directions[0] == 1 && directions[dir_count-1] == 5) {
                is_C = true;
            }
        }
    }

    if (is_A) return GESTURE_LETTER_A;
    if (is_V) return GESTURE_LETTER_V;
    if (is_Z) return GESTURE_LETTER_Z;
    if (is_C) return GESTURE_LETTER_C;

    return GESTURE_LETTER_UNKNOWN;
}

static gesture_type_t get_letter_gesture(gesture_letter_t letter)
{
    switch (letter) {
        case GESTURE_LETTER_A: return GESTURE_TYPE_LETTER_A;
        case GESTURE_LETTER_B: return GESTURE_TYPE_LETTER_B;
        case GESTURE_LETTER_C: return GESTURE_TYPE_LETTER_C;
        case GESTURE_LETTER_V: return GESTURE_TYPE_LETTER_V;
        case GESTURE_LETTER_Z: return GESTURE_TYPE_LETTER_Z;
        default: return GESTURE_TYPE_UNKNOWN;
    }
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
        power_manager_acquire_freq_lock();
        power_manager_reset_activity_timer();
        lv_indev_get_point(indev, &s_start_point);
        s_press_time = lv_tick_get();
        s_is_pressed = true;
        s_is_drawing = true;
        clear_trace();
        s_gesture_trace.start_time = s_press_time;
        add_point(s_start_point);
    } else if (code == LV_EVENT_PRESSING) {
        if (!s_is_pressed || !s_is_drawing) return;
        
        lv_point_t current_point;
        lv_indev_get_point(indev, &current_point);
        
        static int move_count = 0;
        move_count++;
        
        if (move_count % GESTURE_SAMPLE_INTERVAL == 0) {
            add_point(current_point);
        }
    } else if (code == LV_EVENT_RELEASED) {
        if (!s_is_pressed) return;

        lv_indev_get_point(indev, &s_end_point);
        uint32_t release_time = lv_tick_get();
        uint32_t press_duration = release_time - s_press_time;

        add_point(s_end_point);

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
                if (s_gesture_trace.count >= 10) {
                    gesture_letter_t letter = recognize_letter();
                    if (letter != GESTURE_LETTER_UNKNOWN) {
                        gesture_type_t gtype = get_letter_gesture(letter);
                        if (s_callback && gtype != GESTURE_TYPE_UNKNOWN) {
                            s_callback(gtype, s_start_point, s_end_point);
                            ESP_LOGI(TAG, "Letter gesture recognized: %d", letter);
                            s_is_drawing = false;
                            clear_trace();
                            s_is_pressed = false;
                            return;
                        }
                    }
                }
                handle_swipe(s_start_point, s_end_point);
            }
        }

        s_is_pressed = false;
        s_is_drawing = false;
        power_manager_release_freq_lock();
    }
}