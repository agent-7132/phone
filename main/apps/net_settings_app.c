#include "net_settings_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "net_manager.h"
#include "input_tool.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "NET_SETTINGS";

#define MAX_CONNECT_HISTORY 5
#define HISTORY_KEY "wifi_history"

typedef struct {
    char ssid[32];
    char password[64];
    int8_t rssi;
    int last_connected;
} wifi_history_t;

static lv_obj_t *status_label;
static lv_obj_t *ip_label;
static lv_obj_t *ssid_label;
static lv_obj_t *ap_list;
static lv_obj_t *password_input;
static lv_obj_t *connect_btn;
static lv_obj_t *history_list;
static input_tool_t *input_tool;
static wifi_history_t s_wifi_history[MAX_CONNECT_HISTORY];
static int s_history_count = 0;

static void back_button_cb(lv_event_t *e);
static void refresh_status(void);
static void select_ap_cb(lv_event_t *e);
static void scan_wifi_cb(lv_event_t *e);
static void connect_wifi_cb(lv_event_t *e);
static void disconnect_wifi_cb(lv_event_t *e);
static void start_ethernet_cb(lv_event_t *e);
static void select_history_cb(lv_event_t *e);
static void clear_history_cb(lv_event_t *e);
static void load_wifi_history(void);
static void save_wifi_history(const char *ssid, const char *password);
static void net_event_callback(net_info_t *info);

static void load_wifi_history(void)
{
    s_history_count = 0;
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("net_settings", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return;
    }

    size_t len = sizeof(s_wifi_history);
    ret = nvs_get_blob(nvs_handle, HISTORY_KEY, s_wifi_history, &len);
    if (ret == ESP_OK) {
        s_history_count = len / sizeof(wifi_history_t);
        if (s_history_count > MAX_CONNECT_HISTORY) {
            s_history_count = MAX_CONNECT_HISTORY;
        }
        ESP_LOGI(TAG, "Loaded %d WiFi history entries", s_history_count);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No WiFi history found");
    } else {
        ESP_LOGW(TAG, "NVS get_blob failed: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
}

static void save_wifi_history(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        return;
    }

    for (int i = 0; i < s_history_count; i++) {
        if (strcmp(s_wifi_history[i].ssid, ssid) == 0) {
            memmove(&s_wifi_history[i], &s_wifi_history[i + 1],
                    (s_history_count - i - 1) * sizeof(wifi_history_t));
            s_history_count--;
            break;
        }
    }

    if (s_history_count >= MAX_CONNECT_HISTORY) {
        s_history_count = MAX_CONNECT_HISTORY - 1;
    }

    memmove(&s_wifi_history[1], &s_wifi_history[0],
            s_history_count * sizeof(wifi_history_t));

    memset(&s_wifi_history[0], 0, sizeof(wifi_history_t));
    strncpy(s_wifi_history[0].ssid, ssid, sizeof(s_wifi_history[0].ssid) - 1);
    if (password) {
        strncpy(s_wifi_history[0].password, password, sizeof(s_wifi_history[0].password) - 1);
    }
    s_wifi_history[0].last_connected = xTaskGetTickCount() / portTICK_PERIOD_MS;
    s_history_count++;

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("net_settings", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_blob(nvs_handle, HISTORY_KEY, s_wifi_history,
                       s_history_count * sizeof(wifi_history_t));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS set_blob failed: %s", esp_err_to_name(ret));
    } else {
        ret = nvs_commit(nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "NVS commit failed: %s", esp_err_to_name(ret));
        }
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Saved WiFi history: %s", ssid);
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

static void refresh_status(void)
{
    net_info_t *info = net_manager_get_info();
    
    char status_str[64];
    char ip_str[32];
    char ssid_str[64];
    
    switch (info->state) {
        case NET_STATE_DISCONNECTED:
            snprintf(status_str, sizeof(status_str), "Status: Disconnected");
            break;
        case NET_STATE_CONNECTING:
            snprintf(status_str, sizeof(status_str), "Status: Connecting...");
            break;
        case NET_STATE_CONNECTED:
            snprintf(status_str, sizeof(status_str), "Status: Connected");
            break;
        case NET_STATE_FAILED:
            snprintf(status_str, sizeof(status_str), "Status: Connection Failed");
            break;
        default:
            snprintf(status_str, sizeof(status_str), "Status: Unknown");
            break;
    }
    
    switch (info->type) {
        case NET_TYPE_WIFI:
            snprintf(ip_str, sizeof(ip_str), "IP: %s", info->ip[0] ? info->ip : "--");
            snprintf(ssid_str, sizeof(ssid_str), "SSID: %s", info->ssid[0] ? info->ssid : "--");
            break;
        case NET_TYPE_ETHERNET:
            snprintf(ip_str, sizeof(ip_str), "IP: %s", info->ip[0] ? info->ip : "--");
            snprintf(ssid_str, sizeof(ssid_str), "Type: Ethernet");
            break;
        default:
            snprintf(ip_str, sizeof(ip_str), "IP: --");
            snprintf(ssid_str, sizeof(ssid_str), "SSID: --");
            break;
    }
    
    lv_label_set_text(status_label, status_str);
    lv_label_set_text(ip_label, ip_str);
    lv_label_set_text(ssid_label, ssid_str);

    if (info->state == NET_STATE_CONNECTED) {
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    } else if (info->state == NET_STATE_CONNECTING) {
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    } else if (info->state == NET_STATE_FAILED) {
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
    } else {
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    }
}

static void select_ap_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *ssid = (char *)lv_obj_get_user_data(btn);
    
    for (int i = 0; i < s_history_count; i++) {
        if (strcmp(s_wifi_history[i].ssid, ssid) == 0) {
            lv_textarea_set_text(password_input, s_wifi_history[i].password);
            break;
        }
    }
    
    lv_textarea_set_placeholder_text(password_input, "Enter password");
    lv_obj_clear_flag(password_input, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_set_user_data(connect_btn, ssid);
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Selected: %s", ssid);
    lv_label_set_text(status_label, msg);
}

static void scan_wifi_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Scanning WiFi networks");
    
    lv_obj_clean(ap_list);
    
    net_ap_info_t ap_list_buf[MAX_AP_RECORDS];
    int ap_count = net_manager_wifi_scan(ap_list_buf, MAX_AP_RECORDS);
    
    if (ap_count < 0) {
        lv_obj_t *error_label = lv_label_create(ap_list);
        lv_label_set_text(error_label, "Scan failed");
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xFF0000), 0);
        lv_obj_center(error_label);
        return;
    }

    if (ap_count == 0) {
        lv_obj_t *empty_label = lv_label_create(ap_list);
        lv_label_set_text(empty_label, "No WiFi networks found");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
        return;
    }

    for (int i = 0; i < ap_count; i++) {
        lv_obj_t *btn = lv_btn_create(ap_list);
        lv_obj_set_size(btn, 360, 50);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        
        lv_obj_t *row = lv_obj_create(btn);
        lv_obj_set_size(row, 350, 40);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_center(row);
        
        lv_obj_t *name_label = lv_label_create(row);
        lv_label_set_text(name_label, ap_list_buf[i].ssid);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 10, 0);
        
        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), "%d dBm", ap_list_buf[i].rssi);
        lv_obj_t *rssi_label = lv_label_create(row);
        lv_label_set_text(rssi_label, rssi_str);
        lv_obj_set_style_text_font(rssi_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(rssi_label, lv_color_hex(0x87CEEB), 0);
        lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, -10, 0);
        
        char *ssid_copy = (char *)heap_caps_malloc(strlen(ap_list_buf[i].ssid) + 1, MALLOC_CAP_SPIRAM);
        strcpy(ssid_copy, ap_list_buf[i].ssid);
        lv_obj_set_user_data(btn, ssid_copy);
        
        lv_obj_add_event_cb(btn, select_ap_cb, LV_EVENT_CLICKED, NULL);
    }
}

static void connect_wifi_cb(lv_event_t *e)
{
    char *ssid = (char *)lv_obj_get_user_data(connect_btn);
    const char *password = lv_textarea_get_text(password_input);
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    lv_label_set_text(status_label, "Connecting...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    
    esp_err_t ret = net_manager_wifi_connect(ssid, password);
    
    if (ret == ESP_OK) {
        save_wifi_history(ssid, password);
        refresh_status();
    } else {
        lv_label_set_text(status_label, "Connection failed");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
    }
    
    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
}

static void disconnect_wifi_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Disconnecting WiFi");
    
    esp_err_t ret = net_manager_wifi_disconnect();
    if (ret == ESP_OK) {
        lv_label_set_text(status_label, "Disconnected");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
        refresh_status();
    } else {
        lv_label_set_text(status_label, "Disconnect failed");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
    }
}

static void start_ethernet_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Starting Ethernet");
    
    esp_err_t ret = net_manager_ethernet_start();
    if (ret == ESP_OK) {
        lv_label_set_text(status_label, "Ethernet starting...");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    } else {
        lv_label_set_text(status_label, "Ethernet start failed");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0);
    }
}

static void select_history_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (idx >= 0 && idx < s_history_count) {
        const char *ssid = s_wifi_history[idx].ssid;
        const char *password = s_wifi_history[idx].password;
        
        lv_textarea_set_text(password_input, password);
        lv_textarea_set_placeholder_text(password_input, "Enter password");
        lv_obj_clear_flag(password_input, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
        
        char *ssid_copy = (char *)heap_caps_malloc(strlen(ssid) + 1, MALLOC_CAP_SPIRAM);
        strcpy(ssid_copy, ssid);
        lv_obj_set_user_data(connect_btn, ssid_copy);
        
        char msg[64];
        snprintf(msg, sizeof(msg), "Selected: %s", ssid);
        lv_label_set_text(status_label, msg);
    }
}

static void clear_history_cb(lv_event_t *e)
{
    s_history_count = 0;
    memset(s_wifi_history, 0, sizeof(s_wifi_history));
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("net_settings", NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_key(nvs_handle, HISTORY_KEY);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    lv_obj_clean(history_list);
    lv_obj_t *empty_label = lv_label_create(history_list);
    lv_label_set_text(empty_label, "No saved networks");
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
    lv_obj_center(empty_label);
    
    ESP_LOGI(TAG, "WiFi history cleared");
}

static void net_event_callback(net_info_t *info)
{
    refresh_status();
}

static void show_history(void)
{
    lv_obj_clean(history_list);
    
    if (s_history_count == 0) {
        lv_obj_t *empty_label = lv_label_create(history_list);
        lv_label_set_text(empty_label, "No saved networks");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
        return;
    }

    for (int i = 0; i < s_history_count; i++) {
        lv_obj_t *btn = lv_btn_create(history_list);
        lv_obj_set_size(btn, 340, 45);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        
        lv_obj_t *row = lv_obj_create(btn);
        lv_obj_set_size(row, 330, 35);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_center(row);
        
        lv_obj_t *name_label = lv_label_create(row);
        lv_label_set_text(name_label, s_wifi_history[i].ssid);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_t *arrow_label = lv_label_create(row);
        lv_label_set_text(arrow_label, "→");
        lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(arrow_label, lv_color_hex(0x87CEEB), 0);
        lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, -10, 0);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, select_history_cb, LV_EVENT_CLICKED, NULL);
    }
}

lv_obj_t *net_settings_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Network Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Status: --");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 20, 85);
    
    ip_label = lv_label_create(scr);
    lv_label_set_text(ip_label, "IP: --");
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 20, 110);
    
    ssid_label = lv_label_create(scr);
    lv_label_set_text(ssid_label, "SSID: --");
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 20, 135);
    
    lv_obj_t *history_title = lv_label_create(scr);
    lv_label_set_text(history_title, "Saved Networks");
    lv_obj_set_style_text_font(history_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(history_title, lv_color_hex(0x8888AA), 0);
    lv_obj_align(history_title, LV_ALIGN_TOP_LEFT, 20, 170);
    
    history_list = lv_obj_create(scr);
    lv_obj_set_size(history_list, 380, 130);
    lv_obj_set_style_bg_color(history_list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(history_list, 1, 0);
    lv_obj_set_style_border_color(history_list, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(history_list, 8, 0);
    lv_obj_align(history_list, LV_ALIGN_TOP_MID, 0, 195);
    
    lv_obj_t *clear_history_btn = lv_btn_create(scr);
    lv_obj_set_size(clear_history_btn, 80, 30);
    lv_obj_set_style_bg_color(clear_history_btn, lv_color_hex(0x6a4a4a), 0);
    lv_obj_set_style_bg_color(clear_history_btn, lv_color_hex(0x7a5a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(clear_history_btn, 0, 0);
    lv_obj_set_style_radius(clear_history_btn, 4, 0);
    lv_obj_align(clear_history_btn, LV_ALIGN_TOP_RIGHT, -20, 185);
    lv_obj_add_event_cb(clear_history_btn, clear_history_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *clear_label = lv_label_create(clear_history_btn);
    lv_label_set_text(clear_label, "Clear");
    lv_obj_set_style_text_font(clear_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(clear_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(clear_label);
    
    lv_obj_t *wifi_title = lv_label_create(scr);
    lv_label_set_text(wifi_title, "WiFi Networks");
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_title, lv_color_hex(0x8888AA), 0);
    lv_obj_align(wifi_title, LV_ALIGN_TOP_LEFT, 20, 340);
    
    lv_obj_t *scan_btn = lv_btn_create(scr);
    lv_obj_set_size(scan_btn, 100, 40);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(scan_btn, 0, 0);
    lv_obj_set_style_radius(scan_btn, 8, 0);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -20, 335);
    lv_obj_add_event_cb(scan_btn, scan_wifi_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scan_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(scan_label);
    
    ap_list = lv_obj_create(scr);
    lv_obj_set_size(ap_list, 380, 150);
    lv_obj_set_style_bg_color(ap_list, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_width(ap_list, 1, 0);
    lv_obj_set_style_border_color(ap_list, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(ap_list, 8, 0);
    lv_obj_align(ap_list, LV_ALIGN_TOP_MID, 0, 375);
    
    password_input = lv_textarea_create(scr);
    lv_obj_set_size(password_input, 300, 50);
    lv_textarea_set_placeholder_text(password_input, "Enter password");
    lv_obj_set_style_bg_color(password_input, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(password_input, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_border_width(password_input, 1, 0);
    lv_obj_set_style_text_color(password_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(password_input, LV_ALIGN_TOP_MID, 0, 540);
    lv_obj_add_flag(password_input, LV_OBJ_FLAG_HIDDEN);
    
    connect_btn = lv_btn_create(scr);
    lv_obj_set_size(connect_btn, 100, 45);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(connect_btn, 0, 0);
    lv_obj_set_style_radius(connect_btn, 8, 0);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_MID, 0, 600);
    lv_obj_add_event_cb(connect_btn, connect_wifi_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(connect_btn, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(connect_label);
    
    lv_obj_t *disconnect_btn = lv_btn_create(scr);
    lv_obj_set_size(disconnect_btn, 150, 45);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0x8a4a4a), 0);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0x9a5a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(disconnect_btn, 0, 0);
    lv_obj_set_style_radius(disconnect_btn, 8, 0);
    lv_obj_align(disconnect_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(disconnect_btn, disconnect_wifi_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *disconnect_label = lv_label_create(disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(disconnect_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(disconnect_label);
    
    lv_obj_t *eth_btn = lv_btn_create(scr);
    lv_obj_set_size(eth_btn, 150, 45);
    lv_obj_set_style_bg_color(eth_btn, lv_color_hex(0x4a6a8a), 0);
    lv_obj_set_style_bg_color(eth_btn, lv_color_hex(0x5a7a9a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(eth_btn, 0, 0);
    lv_obj_set_style_radius(eth_btn, 8, 0);
    lv_obj_align(eth_btn, LV_ALIGN_BOTTOM_LEFT, 180, -20);
    lv_obj_add_event_cb(eth_btn, start_ethernet_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *eth_label = lv_label_create(eth_btn);
    lv_label_set_text(eth_label, "Ethernet");
    lv_obj_set_style_text_font(eth_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(eth_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(eth_label);
    
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);
    
    input_tool = input_tool_create(scr);
    input_tool_attach_textarea(input_tool, password_input);
    
    load_wifi_history();
    show_history();
    refresh_status();
    
    net_manager_register_callback(net_event_callback);
    
    ESP_LOGI(TAG, "Network settings app created");
    return scr;
}