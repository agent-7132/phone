#include "sdcard_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "SDCARD_APP";

static lv_obj_t *file_list = NULL;
static lv_obj_t *status_label = NULL;

static void list_files(const char *path)
{
    if (!file_list) return;
    
    lv_obj_clean(file_list);
    
    DIR *dir = opendir(path);
    if (!dir) {
        lv_label_set_text(status_label, "Cannot open directory");
        return;
    }
    
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL && count < 50) {
        if (entry->d_name[0] == '.') continue;
        
        lv_obj_t *btn = lv_btn_create(file_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, entry->d_name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        count++;
    }
    
    closedir(dir);
    
    char status[64];
    snprintf(status, sizeof(status), "Files: %d", count);
    lv_label_set_text(status_label, status);
}

static void mount_sdcard(lv_event_t *e)
{
    esp_err_t ret = bsp_sdcard_mount();
    
    if (ret == ESP_OK) {
        lv_label_set_text(status_label, "SD Card mounted");
        list_files("/sdcard");
        ESP_LOGI(TAG, "SD Card mounted successfully");
    } else {
        lv_label_set_text(status_label, "SD Card mount failed");
        ESP_LOGE(TAG, "SD Card mount failed: %s", esp_err_to_name(ret));
    }
}

static void unmount_sdcard(lv_event_t *e)
{
    esp_err_t ret = bsp_sdcard_unmount();
    
    if (ret == ESP_OK) {
        lv_label_set_text(status_label, "SD Card unmounted");
        lv_obj_clean(file_list);
        ESP_LOGI(TAG, "SD Card unmounted");
    } else {
        lv_label_set_text(status_label, "Unmount failed");
        ESP_LOGE(TAG, "SD Card unmount failed: %s", esp_err_to_name(ret));
    }
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *sdcard_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "SD Card Browser");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Status: Not mounted");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *mount_btn = lv_btn_create(scr);
    lv_obj_set_size(mount_btn, 100, 40);
    lv_obj_set_style_bg_color(mount_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(mount_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(mount_btn, 0, 0);
    lv_obj_set_style_radius(mount_btn, 8, 0);
    lv_obj_align(mount_btn, LV_ALIGN_TOP_RIGHT, -20, 50);
    lv_obj_add_event_cb(mount_btn, mount_sdcard, LV_EVENT_CLICKED, NULL);

    lv_obj_t *mount_label = lv_label_create(mount_btn);
    lv_label_set_text(mount_label, "Mount");
    lv_obj_set_style_text_font(mount_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mount_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(mount_label);

    lv_obj_t *unmount_btn = lv_btn_create(scr);
    lv_obj_set_size(unmount_btn, 100, 40);
    lv_obj_set_style_bg_color(unmount_btn, lv_color_hex(0x6a4a4a), 0);
    lv_obj_set_style_bg_color(unmount_btn, lv_color_hex(0x7a5a5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(unmount_btn, 0, 0);
    lv_obj_set_style_radius(unmount_btn, 8, 0);
    lv_obj_align(unmount_btn, LV_ALIGN_TOP_RIGHT, -20, 100);
    lv_obj_add_event_cb(unmount_btn, unmount_sdcard, LV_EVENT_CLICKED, NULL);

    lv_obj_t *unmount_label = lv_label_create(unmount_btn);
    lv_label_set_text(unmount_label, "Unmount");
    lv_obj_set_style_text_font(unmount_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(unmount_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(unmount_label);

    lv_obj_t *container = lv_obj_create(scr);
    lv_obj_set_size(container, 440, 350);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(container, 10, 0);
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 40);

    file_list = lv_list_create(container);
    lv_obj_set_size(file_list, 420, 330);
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "SD Card app created");
    return scr;
}
