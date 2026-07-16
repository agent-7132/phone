#include "notification_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NOTIFICATION_MANAGER";

static notification_t notifications[MAX_NOTIFICATIONS];
static int notification_count = 0;
static lv_obj_t *notification_center = NULL;
static lv_obj_t *notification_list = NULL;
static lv_obj_t *unread_dot = NULL;

static void update_notification_list(void);
static void close_center_cb(lv_event_t *e);
static void notification_click_cb(lv_event_t *e);

esp_err_t notification_manager_init(void)
{
    memset(notifications, 0, sizeof(notifications));
    notification_count = 0;
    notification_center = NULL;
    notification_list = NULL;
    ESP_LOGI(TAG, "Notification manager initialized");
    return ESP_OK;
}

void notification_manager_add(const char *title, const char *message, notification_type_t type)
{
    if (notification_count >= MAX_NOTIFICATIONS) {
        for (int i = 0; i < notification_count - 1; i++) {
            notifications[i] = notifications[i + 1];
        }
        notification_count--;
    }

    strncpy(notifications[notification_count].title, title, MAX_NOTIFICATION_TITLE - 1);
    strncpy(notifications[notification_count].message, message, MAX_NOTIFICATION_MESSAGE - 1);
    notifications[notification_count].type = type;
    notifications[notification_count].timestamp = 0;
    notifications[notification_count].read = false;
    notification_count++;

    ESP_LOGI(TAG, "Added notification: %s", title);

    if (notification_center && !lv_obj_has_flag(notification_center, LV_OBJ_FLAG_HIDDEN)) {
        update_notification_list();
    }
}

void notification_manager_remove(int index)
{
    if (index < 0 || index >= notification_count) return;

    for (int i = index; i < notification_count - 1; i++) {
        notifications[i] = notifications[i + 1];
    }
    notification_count--;

    if (notification_center && !lv_obj_has_flag(notification_center, LV_OBJ_FLAG_HIDDEN)) {
        update_notification_list();
    }
}

void notification_manager_clear_all(void)
{
    notification_count = 0;
    memset(notifications, 0, sizeof(notifications));

    if (notification_center && !lv_obj_has_flag(notification_center, LV_OBJ_FLAG_HIDDEN)) {
        update_notification_list();
    }
}

int notification_manager_get_count(void)
{
    return notification_count;
}

int notification_manager_get_unread_count(void)
{
    int count = 0;
    for (int i = 0; i < notification_count; i++) {
        if (!notifications[i].read) count++;
    }
    return count;
}

const notification_t *notification_manager_get(int index)
{
    if (index < 0 || index >= notification_count) return NULL;
    return &notifications[index];
}

void notification_manager_mark_read(int index)
{
    if (index < 0 || index >= notification_count) return;
    notifications[index].read = true;

    if (notification_center && !lv_obj_has_flag(notification_center, LV_OBJ_FLAG_HIDDEN)) {
        update_notification_list();
    }
}

static lv_color_t get_notification_color(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_INFO: return lv_color_hex(0x4a90d9);
        case NOTIFICATION_TYPE_WARNING: return lv_color_hex(0xFFA500);
        case NOTIFICATION_TYPE_ERROR: return lv_color_hex(0xFF4444);
        case NOTIFICATION_TYPE_SUCCESS: return lv_color_hex(0x4CAF50);
        default: return lv_color_hex(0x4a90d9);
    }
}

static const char *get_notification_icon(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_INFO: return "ℹ️";
        case NOTIFICATION_TYPE_WARNING: return "⚠️";
        case NOTIFICATION_TYPE_ERROR: return "❌";
        case NOTIFICATION_TYPE_SUCCESS: return "✅";
        default: return "📌";
    }
}

static void update_notification_list(void)
{
    if (!notification_list) return;

    lv_obj_clean(notification_list);

    if (notification_count == 0) {
        lv_obj_t *empty_label = lv_label_create(notification_list);
        lv_label_set_text(empty_label, "No notifications");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
        return;
    }

    for (int i = notification_count - 1; i >= 0; i--) {
        lv_obj_t *item = lv_obj_create(notification_list);
        lv_obj_set_size(item, 360, 80);
        lv_obj_set_style_bg_color(item, notifications[i].read ? lv_color_hex(0x252540) : lv_color_hex(0x2d2d44), 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 8, 0);
        lv_obj_set_user_data(item, (void *)(intptr_t)i);
        lv_obj_add_event_cb(item, notification_click_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *icon_label = lv_label_create(item);
        lv_label_set_text(icon_label, get_notification_icon(notifications[i].type));
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t *title_label = lv_label_create(item);
        lv_label_set_text(title_label, notifications[i].title);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 50, 10);

        lv_obj_t *msg_label = lv_label_create(item);
        lv_label_set_text(msg_label, notifications[i].message);
        lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(msg_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(msg_label, LV_ALIGN_BOTTOM_LEFT, 50, -10);

        if (!notifications[i].read) {
            lv_obj_t *dot = lv_obj_create(item);
            lv_obj_set_size(dot, 8, 8);
            lv_obj_set_style_bg_color(dot, get_notification_color(notifications[i].type), 0);
            lv_obj_set_style_radius(dot, 4, 0);
            lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, -10, 10);
        }
    }
}

static void close_center_cb(lv_event_t *e)
{
    notification_manager_hide_notification_center();
}

static void notification_click_cb(lv_event_t *e)
{
    lv_obj_t *item = lv_event_get_target(e);
    int index = (intptr_t)lv_obj_get_user_data(item);
    notification_manager_mark_read(index);
}

void notification_manager_show_notification_center(lv_obj_t *parent)
{
    if (!notification_center) {
        notification_center = lv_obj_create(parent);
        lv_obj_set_size(notification_center, 400, 500);
        lv_obj_set_style_bg_color(notification_center, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_color(notification_center, lv_color_hex(0x4a4a6a), 0);
        lv_obj_set_style_border_width(notification_center, 1, 0);
        lv_obj_set_style_radius(notification_center, 10, 0);
        lv_obj_align(notification_center, LV_ALIGN_TOP_MID, 0, 45);
        lv_obj_add_flag(notification_center, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *header = lv_obj_create(notification_center);
        lv_obj_set_size(header, 400, 50);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *title_label = lv_label_create(header);
        lv_label_set_text(title_label, "Notifications");
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 15, 0);

        lv_obj_t *close_btn = lv_btn_create(header);
        lv_obj_set_size(close_btn, 30, 30);
        lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x3a3a5a), 0);
        lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(close_btn, 0, 0);
        lv_obj_set_style_radius(close_btn, 15, 0);
        lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_add_event_cb(close_btn, close_center_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "✕");
        lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(close_label);

        notification_list = lv_obj_create(notification_center);
        lv_obj_set_size(notification_list, 380, 420);
        lv_obj_set_style_bg_color(notification_list, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(notification_list, 0, 0);
        lv_obj_align(notification_list, LV_ALIGN_TOP_MID, 0, 60);
    }

    update_notification_list();
    lv_obj_remove_flag(notification_center, LV_OBJ_FLAG_HIDDEN);
}

void notification_manager_hide_notification_center(void)
{
    if (notification_center) {
        lv_obj_add_flag(notification_center, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *notification_manager_get_center(void)
{
    return notification_center;
}
