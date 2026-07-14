#include "info_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"

static const char *TAG = "INFO_APP";

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *info_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "System Info");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *container = lv_obj_create(scr);
    lv_obj_set_size(container, 420, 450);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(container, 10, 0);
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 40);

    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    char info_text[1024];
    snprintf(info_text, sizeof(info_text), 
             "=== Hardware ===\n"
             "Chip: ESP32-P4\n"
             "CPU: RISC-V\n"
             "Flash: 16MB\n"
             "PSRAM: %dMB\n\n"
             "=== Memory ===\n"
             "Free Heap: %d KB\n"
             "Free PSRAM: %d KB\n\n"
             "=== Software ===\n"
             "ESP-IDF: v%s\n"
             "LVGL: v9.x\n"
             "BSP Version: 2.0.3\n\n"
             "=== Display ===\n"
             "Panel: ST7102 (MIPI-DSI)\n"
             "Resolution: 480x800\n"
             "Color: RGB565\n\n"
             "=== Touch ===\n"
             "Controller: ST7123\n"
             "Interface: I2C\n\n"
             "=== Audio ===\n"
             "Codec: ES8311\n"
             "Speaker: Built-in\n"
             "Microphone: Built-in",
             (int)(total_psram / 1024 / 1024),
             (int)(esp_get_free_heap_size() / 1024),
             (int)(free_psram / 1024),
             IDF_VER);

    lv_obj_t *info_label = lv_label_create(container);
    lv_label_set_text(info_label, info_text);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(info_label, 15, 0);
    lv_obj_set_size(info_label, 400, 430);
    lv_label_set_long_mode(info_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

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

    ESP_LOGI(TAG, "Info app created");
    return scr;
}
