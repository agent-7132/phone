#include "app_store_app.h"
#include "app_manager.h"
#include "app_downloader.h"
#include "app_installer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "APP_STORE_APP";

#define MAX_APPS 20

typedef struct {
    char name[32];
    char title[32];
    char version[16];
    char author[32];
    char description[128];
    char icon[16];
    char download_url[256];
    int size;
    bool installed;
} app_store_item_t;

static lv_obj_t *screen = NULL;
static lv_obj_t *app_list = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *install_btn = NULL;
static lv_obj_t *detail_dialog = NULL;

static app_store_item_t s_apps[MAX_APPS];
static int s_app_count = 0;
static int s_selected_app = -1;

static void load_mock_apps(void)
{
    s_app_count = 0;

    app_store_item_t *app = &s_apps[s_app_count++];
    strcpy(app->name, "sample_app");
    strcpy(app->title, "Sample App");
    strcpy(app->version, "1.0.0");
    strcpy(app->author, "Developer");
    strcpy(app->description, "A sample dynamic application demonstrating the app store functionality");
    strcpy(app->icon, "📱");
    strcpy(app->download_url, "http://example.com/apps/sample_app.epp");
    app->size = 10240;
    app->installed = (app_manager_find_app("sample_app") != NULL);

    app = &s_apps[s_app_count++];
    strcpy(app->name, "game");
    strcpy(app->title, "Puzzle Game");
    strcpy(app->version, "1.2.0");
    strcpy(app->author, "Game Studio");
    strcpy(app->description, "A fun puzzle game to pass the time");
    strcpy(app->icon, "🎮");
    strcpy(app->download_url, "http://example.com/apps/game.epp");
    app->size = 51200;
    app->installed = (app_manager_find_app("game") != NULL);

    app = &s_apps[s_app_count++];
    strcpy(app->name, "weather");
    strcpy(app->title, "Weather");
    strcpy(app->version, "2.1.0");
    strcpy(app->author, "Weather Corp");
    strcpy(app->description, "Check current weather and forecasts");
    strcpy(app->icon, "🌤️");
    strcpy(app->download_url, "http://example.com/apps/weather.epp");
    app->size = 20480;
    app->installed = (app_manager_find_app("weather") != NULL);

    app = &s_apps[s_app_count++];
    strcpy(app->name, "calculator");
    strcpy(app->title, "Calculator");
    strcpy(app->version, "1.0.1");
    strcpy(app->author, "Tools Inc");
    strcpy(app->description, "Simple calculator for basic arithmetic");
    strcpy(app->icon, "🧮");
    strcpy(app->download_url, "http://example.com/apps/calculator.epp");
    app->size = 5120;
    app->installed = (app_manager_find_app("calculator") != NULL);

    app = &s_apps[s_app_count++];
    strcpy(app->name, "clock");
    strcpy(app->title, "Clock");
    strcpy(app->version, "1.5.0");
    strcpy(app->author, "Time Apps");
    strcpy(app->description, "Alarm clock and stopwatch");
    strcpy(app->icon, "⏰");
    strcpy(app->download_url, "http://example.com/apps/clock.epp");
    app->size = 15360;
    app->installed = (app_manager_find_app("clock") != NULL);

    ESP_LOGI(TAG, "Loaded %d mock apps", s_app_count);
}

static char *format_size(int bytes)
{
    static char str[32];
    if (bytes < 1024) {
        sprintf(str, "%d B", bytes);
    } else if (bytes < 1024 * 1024) {
        sprintf(str, "%.1f KB", bytes / 1024.0);
    } else {
        sprintf(str, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
    return str;
}

static void download_progress_callback(int progress, download_state_t state)
{
    char status[64];
    switch (state) {
    case DOWNLOAD_STATE_DOWNLOADING:
        sprintf(status, "Downloading: %d%%", progress);
        lv_label_set_text(status_label, status);
        break;
    case DOWNLOAD_STATE_INSTALLING:
        lv_label_set_text(status_label, "Installing...");
        break;
    case DOWNLOAD_STATE_COMPLETED:
        lv_label_set_text(status_label, "Installed successfully");
        break;
    case DOWNLOAD_STATE_FAILED:
        lv_label_set_text(status_label, "Download failed");
        break;
    default:
        break;
    }
}

static void download_complete_callback(bool success, const char *app_name)
{
    if (success) {
        for (int i = 0; i < s_app_count; i++) {
            if (strcmp(s_apps[i].name, app_name) == 0) {
                s_apps[i].installed = true;
                break;
            }
        }
        if (install_btn) {
            lv_obj_t *btn_label = lv_label_create(install_btn);
            lv_label_set_text(btn_label, "Uninstall");
            lv_obj_set_style_bg_color(install_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
            lv_obj_center(btn_label);
        }
    }
}

static void install_button_cb(lv_event_t *e)
{
    (void)e;
    if (s_selected_app < 0 || s_selected_app >= s_app_count) return;
    
    app_store_item_t *app = &s_apps[s_selected_app];
    
    if (app->installed) {
        install_result_t ret = app_installer_uninstall(app->name);
        if (ret == INSTALL_OK) {
            app->installed = false;
            lv_obj_t *btn_label = lv_label_create(install_btn);
            lv_label_set_text(btn_label, "Install");
            lv_obj_set_style_bg_color(install_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
            lv_obj_center(btn_label);
            lv_label_set_text(status_label, "App uninstalled");
        } else {
            lv_label_set_text(status_label, "Uninstall failed");
        }
    } else {
        lv_label_set_text(status_label, "Downloading...");
        
        app_downloader_start(app->download_url, app->name,
                             download_progress_callback,
                             download_complete_callback);
    }
}

static void close_dialog_cb(lv_event_t *e)
{
    (void)e;
    if (detail_dialog) {
        lv_obj_del(detail_dialog);
        detail_dialog = NULL;
    }
}

static void show_app_detail(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_app_count) return;

    s_selected_app = idx;
    app_store_item_t *app = &s_apps[idx];

    if (detail_dialog) {
        lv_obj_del(detail_dialog);
    }

    detail_dialog = lv_obj_create(screen);
    lv_obj_set_size(detail_dialog, 400, 500);
    lv_obj_set_style_bg_color(detail_dialog, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_border_width(detail_dialog, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(detail_dialog, 16, LV_PART_MAIN);
    lv_obj_center(detail_dialog);

    lv_obj_t *title_label = lv_label_create(detail_dialog);
    char title[64];
    sprintf(title, "%s %s", app->icon, app->title);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *version_label = lv_label_create(detail_dialog);
    sprintf(title, "Version: %s", app->version);
    lv_label_set_text(version_label, title);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, 0);
    lv_obj_align(version_label, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *author_label = lv_label_create(detail_dialog);
    sprintf(title, "by %s", app->author);
    lv_label_set_text(author_label, title);
    lv_obj_set_style_text_color(author_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(author_label, &lv_font_montserrat_14, 0);
    lv_obj_align(author_label, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *desc_label = lv_label_create(detail_dialog);
    lv_label_set_text(desc_label, app->description);
    lv_obj_set_style_text_color(desc_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(desc_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(desc_label, 360);
    lv_obj_align(desc_label, LV_ALIGN_TOP_MID, 0, 110);

    lv_obj_t *size_label = lv_label_create(detail_dialog);
    sprintf(title, "Size: %s", format_size(app->size));
    lv_label_set_text(size_label, title);
    lv_obj_set_style_text_color(size_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(size_label, &lv_font_montserrat_14, 0);
    lv_obj_align(size_label, LV_ALIGN_TOP_MID, 0, 200);

    install_btn = lv_btn_create(detail_dialog);
    lv_obj_set_size(install_btn, 200, 50);
    lv_obj_set_style_bg_color(install_btn, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(install_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(install_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(install_btn, 8, LV_PART_MAIN);
    lv_obj_align(install_btn, LV_ALIGN_BOTTOM_MID, 0, -80);

    lv_obj_t *btn_label = lv_label_create(install_btn);
    if (app->installed) {
        lv_label_set_text(btn_label, "Uninstall");
        lv_obj_set_style_bg_color(install_btn, lv_color_hex(0xa84a4a), LV_PART_MAIN);
        lv_obj_set_style_bg_color(install_btn, lv_color_hex(0xb85a5a), LV_STATE_PRESSED);
    } else {
        lv_label_set_text(btn_label, "Install");
    }
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(install_btn, install_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_btn = lv_btn_create(detail_dialog);
    lv_obj_set_size(close_btn, 100, 40);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(close_btn, 8, LV_PART_MAIN);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
    lv_obj_center(close_label);

    lv_obj_add_event_cb(close_btn, close_dialog_cb, LV_EVENT_CLICKED, NULL);
}

static void update_app_list(void)
{
    if (!app_list) return;
    lv_obj_clean(app_list);

    for (int i = 0; i < s_app_count; i++) {
        app_store_item_t *app = &s_apps[i];

        lv_obj_t *card = lv_obj_create(app_list);
        lv_obj_set_size(card, 440, 80);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x252540), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(card, show_app_detail, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *icon_label = lv_label_create(card);
        lv_label_set_text(icon_label, app->icon);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 15, 0);

        lv_obj_t *info_container = lv_obj_create(card);
        lv_obj_set_size(info_container, 300, 60);
        lv_obj_set_style_bg_opa(info_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(info_container, LV_ALIGN_LEFT_MID, 70, 0);

        lv_obj_t *title_label = lv_label_create(info_container);
        char title[64];
        sprintf(title, "%s  v%s", app->title, app->version);
        lv_label_set_text(title_label, title);
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *author_label = lv_label_create(info_container);
        sprintf(title, "%s | %s", app->author, format_size(app->size));
        lv_label_set_text(author_label, title);
        lv_obj_set_style_text_color(author_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(author_label, &lv_font_montserrat_14, 0);
        lv_obj_align(author_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        lv_obj_t *status_badge = lv_label_create(card);
        if (app->installed) {
            lv_label_set_text(status_badge, "✓ Installed");
            lv_obj_set_style_bg_color(status_badge, lv_color_hex(0x4a8a6a), LV_PART_MAIN);
        } else {
            lv_label_set_text(status_badge, "Free");
            lv_obj_set_style_bg_color(status_badge, lv_color_hex(0x4a4a6a), LV_PART_MAIN);
        }
        lv_obj_set_style_text_color(status_badge, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(status_badge, &lv_font_montserrat_14, 0);
        lv_obj_set_style_pad_all(status_badge, 5, LV_PART_MAIN);
        lv_obj_set_style_radius(status_badge, 4, LV_PART_MAIN);
        lv_obj_align(status_badge, LV_ALIGN_RIGHT_MID, -15, 0);
    }
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    if (detail_dialog) {
        lv_obj_del(detail_dialog);
        detail_dialog = NULL;
    } else {
        app_manager_go_home();
    }
}

lv_obj_t *app_store_app_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a2e), LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "App Store");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 60);

    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "Loading apps...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 100);

    load_mock_apps();

    app_list = lv_obj_create(screen);
    lv_obj_set_size(app_list, 460, 500);
    lv_obj_set_style_bg_color(app_list, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_border_width(app_list, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(app_list, 8, LV_PART_MAIN);
    lv_obj_align(app_list, LV_ALIGN_TOP_MID, 0, 150);

    lv_obj_set_scrollbar_mode(app_list, LV_SCROLLBAR_MODE_ACTIVE);

    update_app_list();

    lv_label_set_text(status_label, "");

    lv_obj_t *back_btn = lv_btn_create(screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(back_btn, 8, LV_PART_MAIN);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);

    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    return screen;
}