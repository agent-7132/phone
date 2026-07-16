#pragma once
#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NOTIFICATIONS 10
#define MAX_NOTIFICATION_TITLE 64
#define MAX_NOTIFICATION_MESSAGE 256

typedef enum {
    NOTIFICATION_TYPE_INFO,
    NOTIFICATION_TYPE_WARNING,
    NOTIFICATION_TYPE_ERROR,
    NOTIFICATION_TYPE_SUCCESS
} notification_type_t;

typedef struct {
    char title[MAX_NOTIFICATION_TITLE];
    char message[MAX_NOTIFICATION_MESSAGE];
    notification_type_t type;
    uint32_t timestamp;
    bool read;
} notification_t;

esp_err_t notification_manager_init(void);
void notification_manager_add(const char *title, const char *message, notification_type_t type);
void notification_manager_remove(int index);
void notification_manager_clear_all(void);
int notification_manager_get_count(void);
int notification_manager_get_unread_count(void);
const notification_t *notification_manager_get(int index);
void notification_manager_mark_read(int index);
void notification_manager_show_notification_center(lv_obj_t *parent);
void notification_manager_hide_notification_center(void);
lv_obj_t *notification_manager_get_center(void);

#ifdef __cplusplus
}
#endif
