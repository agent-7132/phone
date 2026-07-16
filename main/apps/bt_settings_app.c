#include "bt_settings_app.h"
#include "bt_manager.h"
#include "app_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BT_SETTINGS_APP";

#define MAX_DEVICES 20
#define MAX_BONDED_DEVICES 10
#define ADDR_STR_LEN 18

static lv_obj_t *screen = NULL;
static lv_obj_t *device_list = NULL;
static lv_obj_t *bonded_list = NULL;
static lv_obj_t *scan_btn = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *connect_btn = NULL;
static lv_obj_t *toggle_btn = NULL;
static bt_device_t s_devices[MAX_DEVICES];
static int s_device_count = 0;
static int s_selected_device = -1;
static bt_bonded_device_t s_bonded_devices[MAX_BONDED_DEVICES];
static int s_bonded_count = 0;

static char *addr_to_string(uint8_t *addr)
{
    static char str[ADDR_STR_LEN];
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return str;
}

static void device_click_cb(lv_event_t *e);
static void connect_click_cb(lv_event_t *e);
static void disconnect_click_cb(lv_event_t *e);
static void scan_click_cb(lv_event_t *e);
static void toggle_click_cb(lv_event_t *e);
static void bonded_device_click_cb(lv_event_t *e);
static void remove_bond_cb(lv_event_t *e);
static void device_found_cb(bt_device_t *device);
static void scan_complete_cb(void);
static void state_cb(bt_state_t state);
static void connection_cb(bool connected, uint8_t *addr);
static void back_button_cb(lv_event_t *e);
static void load_bonded_devices(void);
static void refresh_bonded_list(void);

static void device_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    for (int i = 0; i < s_device_count; i++) {
        lv_obj_t *btn = lv_obj_get_child(device_list, i);
        if (btn) {
            if (i == idx) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
                s_selected_device = idx;
            } else {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a4a), LV_PART_MAIN);
            }
        }
    }
}

static void connect_click_cb(lv_event_t *e)
{
    (void)e;
    if (s_selected_device < 0 || s_selected_device >= s_device_count) {
        lv_label_set_text(status_label, "Please select a device");
        return;
    }

    bt_device_t *device = &s_devices[s_selected_device];
    char addr_str[ADDR_STR_LEN];
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            device->addr[5], device->addr[4], device->addr[3],
            device->addr[2], device->addr[1], device->addr[0]);

    char status[64];
    sprintf(status, "Connecting to %s...", addr_str);
    lv_label_set_text(status_label, status);

    esp_err_t ret = bt_manager_connect(device->addr, device->addr_type);
    if (ret != ESP_OK) {
        lv_label_set_text(status_label, "Connection failed");
    }
}

static void disconnect_click_cb(lv_event_t *e)
{
    (void)e;
    esp_err_t ret = bt_manager_disconnect();
    if (ret == ESP_OK) {
        lv_label_set_text(status_label, "Disconnecting...");
    }
}

static void scan_click_cb(lv_event_t *e)
{
    (void)e;
    s_device_count = 0;
    s_selected_device = -1;

    if (device_list) {
        lv_obj_clean(device_list);
    }

    lv_label_set_text(status_label, "Scanning...");
    lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
    lv_obj_add_state(connect_btn, LV_STATE_DISABLED);

    esp_err_t ret = bt_manager_start_scan();
    if (ret != ESP_OK) {
        lv_label_set_text(status_label, "Scan failed");
        lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
    }
}

static void toggle_click_cb(lv_event_t *e)
{
    (void)e;
    bt_state_t bt_state = bt_manager_get_state();
    if (bt_state != BT_STATE_DISABLED) {
        bt_manager_disable();
        lv_label_set_text(toggle_btn, "Enable");
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
        lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
        lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
    } else {
        bt_manager_enable();
        lv_label_set_text(toggle_btn, "Disable");
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
        lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
    }
}

static void bonded_device_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (idx >= 0 && idx < s_bonded_count) {
        bt_bonded_device_t *device = &s_bonded_devices[idx];
        
        char addr_str[ADDR_STR_LEN];
        sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                device->addr[5], device->addr[4], device->addr[3],
                device->addr[2], device->addr[1], device->addr[0]);

        char status[64];
        sprintf(status, "Connecting to %s...", addr_str);
        lv_label_set_text(status_label, status);

        esp_err_t ret = bt_manager_connect(device->addr, 0);
        if (ret != ESP_OK) {
            lv_label_set_text(status_label, "Connection failed");
        }
    }
}

static void remove_bond_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (idx >= 0 && idx < s_bonded_count) {
        esp_err_t ret = bt_manager_remove_bond(s_bonded_devices[idx].addr);
        if (ret == ESP_OK) {
            load_bonded_devices();
            refresh_bonded_list();
            lv_label_set_text(status_label, "Bond removed");
        } else {
            lv_label_set_text(status_label, "Failed to remove bond");
        }
    }
}

static void device_found_cb(bt_device_t *device)
{
    if (s_device_count >= MAX_DEVICES) {
        return;
    }

    s_devices[s_device_count] = *device;

    char addr_str[ADDR_STR_LEN];
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            device->addr[5], device->addr[4], device->addr[3],
            device->addr[2], device->addr[1], device->addr[0]);

    char name[32];
    if (strlen(device->name) > 0) {
        strncpy(name, device->name, sizeof(name) - 1);
    } else {
        strcpy(name, "Unknown");
    }

    lv_obj_t *btn = lv_btn_create(device_list);
    lv_obj_set_size(btn, 420, 60);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a4a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, device_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)s_device_count);

    lv_obj_t *label = lv_label_create(btn);
    char label_text[80];
    sprintf(label_text, "%s\n%s  RSSI: %d", name, addr_str, device->rssi);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 15, 0);

    s_device_count++;

    lv_obj_clear_state(connect_btn, LV_STATE_DISABLED);
}

static void scan_complete_cb(void)
{
    lv_label_set_text(status_label, "Scan complete");
    lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
}

static void state_cb(bt_state_t state)
{
    switch (state) {
    case BT_STATE_DISABLED:
        lv_label_set_text(status_label, "Bluetooth disabled");
        lv_label_set_text(toggle_btn, "Enable");
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
        lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
        lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
        break;
    case BT_STATE_IDLE:
        lv_label_set_text(status_label, "Bluetooth ready");
        lv_label_set_text(toggle_btn, "Disable");
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
        lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
        break;
    case BT_STATE_SCANNING:
        lv_label_set_text(status_label, "Scanning...");
        break;
    case BT_STATE_CONNECTING:
        lv_label_set_text(status_label, "Connecting...");
        break;
    case BT_STATE_CONNECTED:
        lv_label_set_text(status_label, "Connected");
        lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
        break;
    case BT_STATE_DISCONNECTING:
        lv_label_set_text(status_label, "Disconnecting...");
        break;
    default:
        lv_label_set_text(status_label, "Unknown");
        break;
    }
}

static void connection_cb(bool connected, uint8_t *addr)
{
    if (connected) {
        char addr_str[ADDR_STR_LEN];
        sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        char status[64];
        sprintf(status, "Connected to %s", addr_str);
        lv_label_set_text(status_label, status);
        lv_obj_add_state(connect_btn, LV_STATE_DISABLED);
        load_bonded_devices();
        refresh_bonded_list();
    } else {
        lv_label_set_text(status_label, "Disconnected");
        lv_obj_clear_state(connect_btn, LV_STATE_DISABLED);
    }
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    bt_manager_stop_scan();
    app_manager_go_home();
}

static void load_bonded_devices(void)
{
    s_bonded_count = bt_manager_get_bonded_devices(s_bonded_devices, MAX_BONDED_DEVICES);
    ESP_LOGI(TAG, "Loaded %d bonded devices", s_bonded_count);
}

static void refresh_bonded_list(void)
{
    lv_obj_clean(bonded_list);
    
    if (s_bonded_count == 0) {
        lv_obj_t *empty_label = lv_label_create(bonded_list);
        lv_label_set_text(empty_label, "No bonded devices");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_center(empty_label);
        return;
    }

    for (int i = 0; i < s_bonded_count; i++) {
        lv_obj_t *item = lv_obj_create(bonded_list);
        lv_obj_set_size(item, 400, 50);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x252540), LV_PART_MAIN);
        lv_obj_set_style_border_width(item, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(item, 6, LV_PART_MAIN);
        
        char name[32];
        if (strlen(s_bonded_devices[i].name) > 0) {
            strncpy(name, s_bonded_devices[i].name, sizeof(name) - 1);
        } else {
            strcpy(name, "Unknown");
        }

        char addr_str[ADDR_STR_LEN];
        sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
                s_bonded_devices[i].addr[5], s_bonded_devices[i].addr[4], s_bonded_devices[i].addr[3],
                s_bonded_devices[i].addr[2], s_bonded_devices[i].addr[1], s_bonded_devices[i].addr[0]);

        lv_obj_t *info_label = lv_label_create(item);
        char label_text[64];
        sprintf(label_text, "%s  %s", name, addr_str);
        lv_label_set_text(info_label, label_text);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_align(info_label, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t *connect_btn_bonded = lv_btn_create(item);
        lv_obj_set_size(connect_btn_bonded, 60, 35);
        lv_obj_set_style_bg_color(connect_btn_bonded, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(connect_btn_bonded, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(connect_btn_bonded, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(connect_btn_bonded, 4, LV_PART_MAIN);
        lv_obj_align(connect_btn_bonded, LV_ALIGN_RIGHT_MID, -70, 0);
        lv_obj_add_event_cb(connect_btn_bonded, bonded_device_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *connect_label = lv_label_create(connect_btn_bonded);
        lv_label_set_text(connect_label, "Conn");
        lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(connect_label);

        lv_obj_t *remove_btn = lv_btn_create(item);
        lv_obj_set_size(remove_btn, 50, 35);
        lv_obj_set_style_bg_color(remove_btn, lv_color_hex(0x8a4a4a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(remove_btn, lv_color_hex(0x9a5a5a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(remove_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(remove_btn, 4, LV_PART_MAIN);
        lv_obj_align(remove_btn, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_add_event_cb(remove_btn, remove_bond_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *remove_label = lv_label_create(remove_btn);
        lv_label_set_text(remove_label, "X");
        lv_obj_set_style_text_font(remove_label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(remove_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(remove_label);
    }
}

lv_obj_t *bt_settings_app_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a2e), LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "Bluetooth Settings");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 50);

    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 90);

    bt_manager_register_state_cb(state_cb);
    bt_manager_register_device_found_cb(device_found_cb);
    bt_manager_register_connection_cb(connection_cb);
    bt_manager_register_scan_complete_cb(scan_complete_cb);

    toggle_btn = lv_btn_create(screen);
    lv_obj_set_size(toggle_btn, 120, 45);
    lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(toggle_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(toggle_btn, 8, LV_PART_MAIN);
    lv_obj_align(toggle_btn, LV_ALIGN_TOP_LEFT, 30, 135);
    lv_obj_add_event_cb(toggle_btn, toggle_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *toggle_label = lv_label_create(toggle_btn);
    lv_label_set_text(toggle_label, "Enable");
    lv_obj_set_style_text_font(toggle_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(toggle_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(toggle_label);

    scan_btn = lv_btn_create(screen);
    lv_obj_set_size(scan_btn, 120, 45);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x4a4a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(scan_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(scan_btn, 8, LV_PART_MAIN);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_RIGHT, -30, 135);
    lv_obj_add_event_cb(scan_btn, scan_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(scan_btn, LV_STATE_DISABLED);

    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(scan_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(scan_label);

    connect_btn = lv_btn_create(screen);
    lv_obj_set_size(connect_btn, 120, 45);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(connect_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(connect_btn, 8, LV_PART_MAIN);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_RIGHT, -160, 135);
    lv_obj_add_event_cb(connect_btn, connect_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_state(connect_btn, LV_STATE_DISABLED);

    lv_obj_t *connect_label = lv_label_create(connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_style_text_font(connect_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(connect_label);

    lv_obj_t *disconnect_btn = lv_btn_create(screen);
    lv_obj_set_size(disconnect_btn, 120, 45);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(disconnect_btn, lv_color_hex(0xb85a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(disconnect_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(disconnect_btn, 8, LV_PART_MAIN);
    lv_obj_align(disconnect_btn, LV_ALIGN_TOP_LEFT, 160, 135);
    lv_obj_add_event_cb(disconnect_btn, disconnect_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *disconnect_label = lv_label_create(disconnect_btn);
    lv_label_set_text(disconnect_label, "Disconnect");
    lv_obj_set_style_text_font(disconnect_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(disconnect_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(disconnect_label);

    lv_obj_t *bonded_title = lv_label_create(screen);
    lv_label_set_text(bonded_title, "Paired Devices");
    lv_obj_set_style_text_font(bonded_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(bonded_title, lv_color_hex(0x8888AA), LV_PART_MAIN);
    lv_obj_align(bonded_title, LV_ALIGN_TOP_LEFT, 30, 190);

    bonded_list = lv_obj_create(screen);
    lv_obj_set_size(bonded_list, 440, 130);
    lv_obj_set_style_bg_color(bonded_list, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_border_width(bonded_list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bonded_list, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_radius(bonded_list, 8, LV_PART_MAIN);
    lv_obj_align(bonded_list, LV_ALIGN_TOP_MID, 0, 215);

    lv_obj_t *devices_title = lv_label_create(screen);
    lv_label_set_text(devices_title, "Available Devices");
    lv_obj_set_style_text_font(devices_title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(devices_title, lv_color_hex(0x8888AA), LV_PART_MAIN);
    lv_obj_align(devices_title, LV_ALIGN_TOP_LEFT, 30, 360);

    device_list = lv_obj_create(screen);
    lv_obj_set_size(device_list, 440, 300);
    lv_obj_set_style_bg_color(device_list, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_border_width(device_list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(device_list, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_radius(device_list, 8, LV_PART_MAIN);
    lv_obj_align(device_list, LV_ALIGN_TOP_MID, 0, 385);

    lv_obj_t *back_btn = lv_btn_create(screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 8, LV_PART_MAIN);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(back_label);

    load_bonded_devices();
    refresh_bonded_list();

    bt_state_t state = bt_manager_get_state();
    if (state == BT_STATE_IDLE) {
        lv_label_set_text(status_label, "Bluetooth ready");
        lv_label_set_text(toggle_btn, "Disable");
        lv_obj_set_style_bg_color(toggle_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
        lv_obj_clear_state(scan_btn, LV_STATE_DISABLED);
    } else {
        lv_label_set_text(status_label, "Bluetooth initializing...");
    }

    return screen;
}