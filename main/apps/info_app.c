#include "info_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "ui_animation.h"
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

void info_app_on_exit(void)
{
    ESP_LOGI(TAG, "Info app exited");
}

static void create_info_card(lv_obj_t *parent, const char *title, const char *text, lv_coord_t y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 420, 110);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 6, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 50, 0);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
    
    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 15, 12);
    
    lv_obj_t *text_label = lv_label_create(card);
    lv_label_set_text(text_label, text);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(text_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_pad_all(text_label, 0, 0);
    lv_obj_set_size(text_label, 390, 70);
    lv_obj_align(text_label, LV_ALIGN_TOP_LEFT, 15, 35);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
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

    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    char hardware_text[128];
    char memory_text[128];
    char software_text[128];
    char display_text[128];
    
    snprintf(hardware_text, sizeof(hardware_text), 
             "Chip: ESP32-P4\nCPU: RISC-V\nFlash: 16MB\nPSRAM: %dMB",
             (int)(total_psram / 1024 / 1024));
    
    snprintf(memory_text, sizeof(memory_text), 
             "Free Heap: %d KB\nFree PSRAM: %d KB",
             (int)(esp_get_free_heap_size() / 1024),
             (int)(free_psram / 1024));
    
    snprintf(software_text, sizeof(software_text), 
             "ESP-IDF: v%s\nLVGL: v9.x\nBSP Version: 2.0.3", IDF_VER);
    
    snprintf(display_text, sizeof(display_text), 
             "Panel: ST7102 (MIPI-DSI)\nResolution: 480x800\nColor: RGB565");

    create_info_card(scr, "⚙ Hardware", hardware_text, 90);
    create_info_card(scr, "💾 Memory", memory_text, 210);
    create_info_card(scr, "📦 Software", software_text, 330);
    create_info_card(scr, "🖥 Display", display_text, 450);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 10, 0);
    lv_obj_set_style_shadow_width(back_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, 40, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    ESP_LOGI(TAG, "Info app created with modern design");
    return scr;
}
