#include "control_center.h"
#include "net_manager.h"
#include "bt_manager.h"
#include "settings_manager.h"
#include "app_manager.h"
#include "status_bar.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CONTROL_CENTER";

static control_center_t s_control_center = {0};

static void close_center_cb(lv_event_t *e);
static void toggle_wifi_cb(lv_event_t *e);
static void toggle_bt_cb(lv_event_t *e);
static void toggle_flashlight_cb(lv_event_t *e);
static void toggle_airplane_cb(lv_event_t *e);
static void volume_changed_cb(lv_event_t *e);
static void brightness_changed_cb(lv_event_t *e);
static void open_net_settings_cb(lv_event_t *e);
static void open_bt_settings_cb(lv_event_t *e);
static void open_settings_app_cb(lv_event_t *e);
static void update_wifi_ui(void);
static void update_bt_ui(void);
static void update_flashlight_ui(void);
static void update_airplane_ui(void);

static void net_status_callback(net_info_t *info)
{
    bool connected = (info->state == NET_STATE_CONNECTED);
    bool enabled = (info->state != NET_STATE_DISCONNECTED || info->type != NET_TYPE_NONE);
    control_center_update_wifi_status(enabled, connected, info->ssid);
    status_bar_set_network((info->type == NET_TYPE_WIFI) ? 1 : 0, connected);
}

static void bt_state_callback(bt_state_t state)
{
    bool enabled = (state != BT_STATE_DISABLED);
    bool connected = (state == BT_STATE_CONNECTED);
    control_center_update_bt_status(enabled, connected, "");
    status_bar_set_bluetooth(enabled, connected);
}

esp_err_t control_center_init(void)
{
    memset(&s_control_center, 0, sizeof(s_control_center));
    net_manager_register_callback(net_status_callback);
    bt_manager_register_state_cb(bt_state_callback);
    ESP_LOGI(TAG, "Control center initialized");
    return ESP_OK;
}

void control_center_show(lv_obj_t *parent)
{
    if (!s_control_center.control_center) {
        s_control_center.control_center = lv_obj_create(parent);
        lv_obj_set_size(s_control_center.control_center, 460, 550);
        lv_obj_set_style_bg_color(s_control_center.control_center, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_border_width(s_control_center.control_center, 0, 0);
        lv_obj_set_style_radius(s_control_center.control_center, 25, 0);
        lv_obj_set_style_shadow_width(s_control_center.control_center, 15, 0);
        lv_obj_set_style_shadow_color(s_control_center.control_center, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(s_control_center.control_center, 150, 0);
        lv_obj_align(s_control_center.control_center, LV_ALIGN_BOTTOM_MID, 0, -15);
        lv_obj_add_flag(s_control_center.control_center, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t *header = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(header, 460, 50);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_radius(header, 25, 0);
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t *title_label = lv_label_create(header);
        lv_label_set_text(title_label, "Control Center");
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 20, 0);

        lv_obj_t *close_btn = lv_btn_create(header);
        lv_obj_set_size(close_btn, 35, 35);
        lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x3a3a5a), 0);
        lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(close_btn, 0, 0);
        lv_obj_set_style_radius(close_btn, 18, 0);
        lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -15, 0);
        lv_obj_add_event_cb(close_btn, close_center_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *close_label = lv_label_create(close_btn);
        lv_label_set_text(close_label, "✕");
        lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(close_label);

        lv_obj_t *quick_actions = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(quick_actions, 440, 110);
        lv_obj_set_style_bg_color(quick_actions, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(quick_actions, 0, 0);
        lv_obj_set_style_border_width(quick_actions, 0, 0);
        lv_obj_align(quick_actions, LV_ALIGN_TOP_MID, 0, 65);

        s_control_center.wifi_btn = lv_btn_create(quick_actions);
        lv_obj_set_size(s_control_center.wifi_btn, 95, 95);
        lv_obj_set_style_bg_color(s_control_center.wifi_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(s_control_center.wifi_btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(s_control_center.wifi_btn, 0, 0);
        lv_obj_set_style_radius(s_control_center.wifi_btn, 20, 0);
        lv_obj_align(s_control_center.wifi_btn, LV_ALIGN_TOP_LEFT, 10, 8);
        lv_obj_add_event_cb(s_control_center.wifi_btn, toggle_wifi_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *wifi_icon = lv_label_create(s_control_center.wifi_btn);
        lv_label_set_text(wifi_icon, "📶");
        lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
        lv_obj_align(wifi_icon, LV_ALIGN_TOP_MID, 0, 5);

        s_control_center.wifi_label = lv_label_create(s_control_center.wifi_btn);
        lv_label_set_text(s_control_center.wifi_label, "WiFi");
        lv_obj_set_style_text_font(s_control_center.wifi_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.wifi_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(s_control_center.wifi_label, LV_ALIGN_BOTTOM_MID, 0, -5);

        s_control_center.bt_btn = lv_btn_create(quick_actions);
        lv_obj_set_size(s_control_center.bt_btn, 95, 95);
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(s_control_center.bt_btn, 0, 0);
        lv_obj_set_style_radius(s_control_center.bt_btn, 20, 0);
        lv_obj_align(s_control_center.bt_btn, LV_ALIGN_TOP_MID, 0, 8);
        lv_obj_add_event_cb(s_control_center.bt_btn, toggle_bt_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *bt_icon = lv_label_create(s_control_center.bt_btn);
        lv_label_set_text(bt_icon, "🔷");
        lv_obj_set_style_text_font(bt_icon, &lv_font_montserrat_14, 0);
        lv_obj_align(bt_icon, LV_ALIGN_TOP_MID, 0, 5);

        s_control_center.bt_label = lv_label_create(s_control_center.bt_btn);
        lv_label_set_text(s_control_center.bt_label, "BT");
        lv_obj_set_style_text_font(s_control_center.bt_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.bt_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(s_control_center.bt_label, LV_ALIGN_BOTTOM_MID, 0, -5);

        s_control_center.flashlight_btn = lv_btn_create(quick_actions);
        lv_obj_set_size(s_control_center.flashlight_btn, 95, 95);
        lv_obj_set_style_bg_color(s_control_center.flashlight_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(s_control_center.flashlight_btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(s_control_center.flashlight_btn, 0, 0);
        lv_obj_set_style_radius(s_control_center.flashlight_btn, 20, 0);
        lv_obj_align(s_control_center.flashlight_btn, LV_ALIGN_TOP_RIGHT, -10, 8);
        lv_obj_add_event_cb(s_control_center.flashlight_btn, toggle_flashlight_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *flashlight_icon = lv_label_create(s_control_center.flashlight_btn);
        lv_label_set_text(flashlight_icon, "🔦");
        lv_obj_set_style_text_font(flashlight_icon, &lv_font_montserrat_14, 0);
        lv_obj_align(flashlight_icon, LV_ALIGN_TOP_MID, 0, 5);

        s_control_center.flashlight_label = lv_label_create(s_control_center.flashlight_btn);
        lv_label_set_text(s_control_center.flashlight_label, "Flash");
        lv_obj_set_style_text_font(s_control_center.flashlight_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.flashlight_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(s_control_center.flashlight_label, LV_ALIGN_BOTTOM_MID, 0, -5);

        s_control_center.airplane_btn = lv_btn_create(quick_actions);
        lv_obj_set_size(s_control_center.airplane_btn, 95, 95);
        lv_obj_set_style_bg_color(s_control_center.airplane_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(s_control_center.airplane_btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(s_control_center.airplane_btn, 0, 0);
        lv_obj_set_style_radius(s_control_center.airplane_btn, 20, 0);
        lv_obj_align(s_control_center.airplane_btn, LV_ALIGN_BOTTOM_LEFT, 10, -8);
        lv_obj_add_event_cb(s_control_center.airplane_btn, toggle_airplane_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *airplane_icon = lv_label_create(s_control_center.airplane_btn);
        lv_label_set_text(airplane_icon, "✈️");
        lv_obj_set_style_text_font(airplane_icon, &lv_font_montserrat_14, 0);
        lv_obj_align(airplane_icon, LV_ALIGN_TOP_MID, 0, 5);

        s_control_center.airplane_label = lv_label_create(s_control_center.airplane_btn);
        lv_label_set_text(s_control_center.airplane_label, "Airplane");
        lv_obj_set_style_text_font(s_control_center.airplane_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.airplane_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(s_control_center.airplane_label, LV_ALIGN_BOTTOM_MID, 0, -5);

        lv_obj_t *network_section = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(network_section, 440, 90);
        lv_obj_set_style_bg_color(network_section, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(network_section, 0, 0);
        lv_obj_set_style_radius(network_section, 15, 0);
        lv_obj_align(network_section, LV_ALIGN_TOP_MID, 0, 195);

        lv_obj_t *network_title = lv_label_create(network_section);
        lv_label_set_text(network_title, "Network Status");
        lv_obj_set_style_text_font(network_title, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(network_title, lv_color_hex(0x8888AA), 0);
        lv_obj_align(network_title, LV_ALIGN_TOP_LEFT, 15, 10);

        s_control_center.wifi_status_label = lv_label_create(network_section);
        lv_label_set_text(s_control_center.wifi_status_label, "WiFi: Not connected");
        lv_obj_set_style_text_font(s_control_center.wifi_status_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.wifi_status_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(s_control_center.wifi_status_label, LV_ALIGN_TOP_LEFT, 15, 35);

        s_control_center.bt_status_label = lv_label_create(network_section);
        lv_label_set_text(s_control_center.bt_status_label, "Bluetooth: Disabled");
        lv_obj_set_style_text_font(s_control_center.bt_status_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.bt_status_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(s_control_center.bt_status_label, LV_ALIGN_TOP_LEFT, 15, 60);

        lv_obj_t *volume_section = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(volume_section, 440, 65);
        lv_obj_set_style_bg_color(volume_section, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(volume_section, 0, 0);
        lv_obj_set_style_radius(volume_section, 15, 0);
        lv_obj_align(volume_section, LV_ALIGN_TOP_MID, 0, 305);

        lv_obj_t *volume_label = lv_label_create(volume_section);
        lv_label_set_text(volume_label, "🔊");
        lv_obj_set_style_text_font(volume_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(volume_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(volume_label, LV_ALIGN_LEFT_MID, 15, 0);

        s_control_center.volume_slider = lv_slider_create(volume_section);
        lv_obj_set_size(s_control_center.volume_slider, 280, 24);
        lv_slider_set_range(s_control_center.volume_slider, 0, 100);
        lv_slider_set_value(s_control_center.volume_slider, settings_manager_get_volume(), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_control_center.volume_slider, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_bg_color(s_control_center.volume_slider, lv_color_hex(0x4a8a6a), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_control_center.volume_slider, lv_color_hex(0x5a9a7a), LV_PART_KNOB);
        lv_obj_set_style_border_width(s_control_center.volume_slider, 0, 0);
        lv_obj_set_style_radius(s_control_center.volume_slider, 12, 0);
        lv_obj_align(s_control_center.volume_slider, LV_ALIGN_LEFT_MID, 50, 0);
        lv_obj_add_event_cb(s_control_center.volume_slider, volume_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

        s_control_center.volume_value_label = lv_label_create(volume_section);
        char vol_str[8];
        snprintf(vol_str, sizeof(vol_str), "%d%%", settings_manager_get_volume());
        lv_label_set_text(s_control_center.volume_value_label, vol_str);
        lv_obj_set_style_text_font(s_control_center.volume_value_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.volume_value_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(s_control_center.volume_value_label, LV_ALIGN_RIGHT_MID, -15, 0);

        lv_obj_t *brightness_section = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(brightness_section, 440, 65);
        lv_obj_set_style_bg_color(brightness_section, lv_color_hex(0x252540), 0);
        lv_obj_set_style_border_width(brightness_section, 0, 0);
        lv_obj_set_style_radius(brightness_section, 15, 0);
        lv_obj_align(brightness_section, LV_ALIGN_TOP_MID, 0, 390);

        lv_obj_t *brightness_label = lv_label_create(brightness_section);
        lv_label_set_text(brightness_label, "☀️");
        lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(brightness_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(brightness_label, LV_ALIGN_LEFT_MID, 15, 0);

        s_control_center.brightness_slider = lv_slider_create(brightness_section);
        lv_obj_set_size(s_control_center.brightness_slider, 280, 24);
        lv_slider_set_range(s_control_center.brightness_slider, 10, 100);
        lv_slider_set_value(s_control_center.brightness_slider, settings_manager_get_brightness(), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_control_center.brightness_slider, lv_color_hex(0x1a1a2e), 0);
        lv_obj_set_style_bg_color(s_control_center.brightness_slider, lv_color_hex(0xFFD700), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_control_center.brightness_slider, lv_color_hex(0xFFE066), LV_PART_KNOB);
        lv_obj_set_style_border_width(s_control_center.brightness_slider, 0, 0);
        lv_obj_set_style_radius(s_control_center.brightness_slider, 12, 0);
        lv_obj_align(s_control_center.brightness_slider, LV_ALIGN_LEFT_MID, 50, 0);
        lv_obj_add_event_cb(s_control_center.brightness_slider, brightness_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

        s_control_center.brightness_value_label = lv_label_create(brightness_section);
        char bri_str[8];
        snprintf(bri_str, sizeof(bri_str), "%d%%", settings_manager_get_brightness());
        lv_label_set_text(s_control_center.brightness_value_label, bri_str);
        lv_obj_set_style_text_font(s_control_center.brightness_value_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_control_center.brightness_value_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(s_control_center.brightness_value_label, LV_ALIGN_RIGHT_MID, -15, 0);

        lv_obj_t *settings_section = lv_obj_create(s_control_center.control_center);
        lv_obj_set_size(settings_section, 440, 50);
        lv_obj_set_style_bg_color(settings_section, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(settings_section, 0, 0);
        lv_obj_set_style_border_width(settings_section, 0, 0);
        lv_obj_align(settings_section, LV_ALIGN_TOP_MID, 0, 475);

        lv_obj_t *settings_btn = lv_btn_create(settings_section);
        lv_obj_set_size(settings_btn, 420, 42);
        lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(settings_btn, 0, 0);
        lv_obj_set_style_radius(settings_btn, 12, 0);
        lv_obj_align(settings_btn, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_event_cb(settings_btn, open_settings_app_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *settings_icon = lv_label_create(settings_btn);
        lv_label_set_text(settings_icon, "⚙️");
        lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_14, 0);
        lv_obj_align(settings_icon, LV_ALIGN_LEFT_MID, 20, 0);

        lv_obj_t *settings_label = lv_label_create(settings_btn);
        lv_label_set_text(settings_label, "Open Settings");
        lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(settings_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(settings_label, LV_ALIGN_LEFT_MID, 50, 0);

        lv_obj_t *settings_arrow = lv_label_create(settings_btn);
        lv_label_set_text(settings_arrow, "›");
        lv_obj_set_style_text_font(settings_arrow, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(settings_arrow, lv_color_hex(0x8888AA), 0);
        lv_obj_align(settings_arrow, LV_ALIGN_RIGHT_MID, -20, 0);

        net_info_t *net_info = net_manager_get_info();
        bool net_connected = (net_info->state == NET_STATE_CONNECTED);
        bool net_enabled = (net_info->state != NET_STATE_DISCONNECTED);
        control_center_update_wifi_status(net_enabled, net_connected, net_info->ssid);

        bt_state_t bt_state = bt_manager_get_state();
        bool bt_enabled = (bt_state != BT_STATE_DISABLED);
        bool bt_connected = (bt_state == BT_STATE_CONNECTED);
        control_center_update_bt_status(bt_enabled, bt_connected, "");
    }

    lv_obj_remove_flag(s_control_center.control_center, LV_OBJ_FLAG_HIDDEN);
}

void control_center_hide(void)
{
    if (s_control_center.control_center) {
        lv_obj_add_flag(s_control_center.control_center, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t *control_center_get(void)
{
    return s_control_center.control_center;
}

void control_center_update_wifi_status(bool enabled, bool connected, const char *ssid)
{
    s_control_center.wifi_enabled = enabled;
    update_wifi_ui();
}

void control_center_update_bt_status(bool enabled, bool connected, const char *device_name)
{
    s_control_center.bt_enabled = enabled;
    update_bt_ui();
}

static void update_wifi_ui(void)
{
    if (!s_control_center.wifi_btn || !s_control_center.wifi_status_label) {
        return;
    }

    net_info_t *info = net_manager_get_info();
    
    if (info->state == NET_STATE_CONNECTED) {
        lv_obj_set_style_bg_color(s_control_center.wifi_btn, lv_color_hex(0x4a8a6a), 0);
        lv_obj_set_style_text_color(s_control_center.wifi_label, lv_color_hex(0x00FF00), 0);
        char status_text[60];
        snprintf(status_text, sizeof(status_text), "WiFi: Connected - %s", info->ssid[0] ? info->ssid : "--");
        lv_label_set_text(s_control_center.wifi_status_label, status_text);
        lv_obj_set_style_text_color(s_control_center.wifi_status_label, lv_color_hex(0x66BB6A), 0);
    } else if (info->state == NET_STATE_CONNECTING) {
        lv_obj_set_style_bg_color(s_control_center.wifi_btn, lv_color_hex(0x4a6a8a), 0);
        lv_obj_set_style_text_color(s_control_center.wifi_label, lv_color_hex(0x87CEEB), 0);
        lv_label_set_text(s_control_center.wifi_status_label, "WiFi: Connecting...");
        lv_obj_set_style_text_color(s_control_center.wifi_status_label, lv_color_hex(0x87CEEB), 0);
    } else {
        lv_obj_set_style_bg_color(s_control_center.wifi_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_text_color(s_control_center.wifi_label, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(s_control_center.wifi_status_label, "WiFi: Not connected");
        lv_obj_set_style_text_color(s_control_center.wifi_status_label, lv_color_hex(0x8888AA), 0);
    }
}

static void update_bt_ui(void)
{
    if (!s_control_center.bt_btn || !s_control_center.bt_status_label) {
        return;
    }

    bt_state_t state = bt_manager_get_state();
    
    if (state == BT_STATE_CONNECTED) {
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x4a8a6a), 0);
        lv_obj_set_style_text_color(s_control_center.bt_label, lv_color_hex(0x4FC3F7), 0);
        lv_label_set_text(s_control_center.bt_status_label, "Bluetooth: Connected");
        lv_obj_set_style_text_color(s_control_center.bt_status_label, lv_color_hex(0x4FC3F7), 0);
    } else if (state == BT_STATE_CONNECTING || state == BT_STATE_SCANNING) {
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x4a6a8a), 0);
        lv_obj_set_style_text_color(s_control_center.bt_label, lv_color_hex(0x87CEEB), 0);
        lv_label_set_text(s_control_center.bt_status_label, state == BT_STATE_SCANNING ? "Bluetooth: Scanning..." : "Bluetooth: Connecting...");
        lv_obj_set_style_text_color(s_control_center.bt_status_label, lv_color_hex(0x87CEEB), 0);
    } else if (state == BT_STATE_IDLE) {
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x3a5a8a), 0);
        lv_obj_set_style_text_color(s_control_center.bt_label, lv_color_hex(0x87CEEB), 0);
        lv_label_set_text(s_control_center.bt_status_label, "Bluetooth: Enabled");
        lv_obj_set_style_text_color(s_control_center.bt_status_label, lv_color_hex(0x87CEEB), 0);
    } else {
        lv_obj_set_style_bg_color(s_control_center.bt_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_text_color(s_control_center.bt_label, lv_color_hex(0xFFFFFF), 0);
        lv_label_set_text(s_control_center.bt_status_label, "Bluetooth: Disabled");
        lv_obj_set_style_text_color(s_control_center.bt_status_label, lv_color_hex(0x8888AA), 0);
    }
}

static void update_flashlight_ui(void)
{
    if (!s_control_center.flashlight_btn || !s_control_center.flashlight_label) {
        return;
    }
    
    if (s_control_center.flashlight_enabled) {
        lv_obj_set_style_bg_color(s_control_center.flashlight_btn, lv_color_hex(0xFFD700), 0);
        lv_obj_set_style_text_color(s_control_center.flashlight_label, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_bg_color(s_control_center.flashlight_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_text_color(s_control_center.flashlight_label, lv_color_hex(0xFFFFFF), 0);
    }
}

static void update_airplane_ui(void)
{
    if (!s_control_center.airplane_btn || !s_control_center.airplane_label) {
        return;
    }
    
    if (s_control_center.airplane_mode) {
        lv_obj_set_style_bg_color(s_control_center.airplane_btn, lv_color_hex(0xFF6B6B), 0);
        lv_obj_set_style_text_color(s_control_center.airplane_label, lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_obj_set_style_bg_color(s_control_center.airplane_btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_text_color(s_control_center.airplane_label, lv_color_hex(0xFFFFFF), 0);
    }
}

static void close_center_cb(lv_event_t *e)
{
    control_center_hide();
}

static void toggle_wifi_cb(lv_event_t *e)
{
    net_info_t *info = net_manager_get_info();
    if (info->state == NET_STATE_CONNECTED) {
        net_manager_wifi_disconnect();
    } else if (info->state == NET_STATE_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi toggle: open settings");
        control_center_hide();
        app_manager_open_app("network");
    }
}

static void toggle_bt_cb(lv_event_t *e)
{
    bt_state_t bt_state = bt_manager_get_state();
    if (bt_state != BT_STATE_DISABLED) {
        bt_manager_disable();
    } else {
        bt_manager_enable();
    }
}

static void toggle_flashlight_cb(lv_event_t *e)
{
    s_control_center.flashlight_enabled = !s_control_center.flashlight_enabled;
    update_flashlight_ui();
    ESP_LOGI(TAG, "Flashlight %s", s_control_center.flashlight_enabled ? "enabled" : "disabled");
}

static void toggle_airplane_cb(lv_event_t *e)
{
    s_control_center.airplane_mode = !s_control_center.airplane_mode;
    update_airplane_ui();
    
    if (s_control_center.airplane_mode) {
        net_manager_wifi_disconnect();
        bt_manager_disable();
    }
    
    ESP_LOGI(TAG, "Airplane mode %s", s_control_center.airplane_mode ? "enabled" : "disabled");
}

static void volume_changed_cb(lv_event_t *e)
{
    int volume = lv_slider_get_value(s_control_center.volume_slider);
    settings_manager_set_volume(volume);
    if (s_control_center.volume_value_label) {
        char vol_str[8];
        snprintf(vol_str, sizeof(vol_str), "%d%%", volume);
        lv_label_set_text(s_control_center.volume_value_label, vol_str);
    }
}

static void brightness_changed_cb(lv_event_t *e)
{
    int brightness = lv_slider_get_value(s_control_center.brightness_slider);
    settings_manager_set_brightness(brightness);
    if (s_control_center.brightness_value_label) {
        char bri_str[8];
        snprintf(bri_str, sizeof(bri_str), "%d%%", brightness);
        lv_label_set_text(s_control_center.brightness_value_label, bri_str);
    }
}

static void open_net_settings_cb(lv_event_t *e)
{
    control_center_hide();
    app_manager_open_app("network");
}

static void open_bt_settings_cb(lv_event_t *e)
{
    control_center_hide();
    app_manager_open_app("bluetooth");
}

static void open_settings_app_cb(lv_event_t *e)
{
    control_center_hide();
    app_manager_open_app("settings");
}