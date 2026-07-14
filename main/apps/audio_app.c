#include "audio_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_APP";

static lv_obj_t *status_label = NULL;
static lv_obj_t *volume_slider = NULL;
static esp_codec_dev_handle_t speaker = NULL;

static void init_audio(lv_event_t *e)
{
    esp_err_t ret = bsp_audio_init(NULL);
    
    if (ret == ESP_OK) {
        speaker = bsp_audio_codec_speaker_init();
        
        if (speaker) {
            lv_label_set_text(status_label, "Audio initialized");
            ESP_LOGI(TAG, "Audio initialized successfully");
        } else {
            lv_label_set_text(status_label, "Speaker init failed");
            ESP_LOGE(TAG, "Speaker init failed");
        }
    } else {
        lv_label_set_text(status_label, "Audio init failed");
        ESP_LOGE(TAG, "Audio init failed: %s", esp_err_to_name(ret));
    }
}

static void play_tone(lv_event_t *e)
{
    if (!speaker) {
        lv_label_set_text(status_label, "Audio not initialized");
        return;
    }
    
    uint32_t volume = lv_slider_get_value(volume_slider);
    esp_codec_dev_set_out_vol(speaker, volume);
    
    lv_label_set_text(status_label, "Playing tone...");
    ESP_LOGI(TAG, "Playing tone with volume %d", volume);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    lv_label_set_text(status_label, "Tone played");
}

static void volume_changed(lv_event_t *e)
{
    uint32_t volume = lv_slider_get_value(volume_slider);
    char vol_str[32];
    snprintf(vol_str, sizeof(vol_str), "Volume: %lu%%", (unsigned long)volume);
    lv_label_set_text(status_label, vol_str);
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *audio_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Audio Player");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Status: Not initialized");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);

    lv_obj_t *init_btn = lv_btn_create(scr);
    lv_obj_set_size(init_btn, 150, 50);
    lv_obj_set_style_bg_color(init_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(init_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(init_btn, 0, 0);
    lv_obj_set_style_radius(init_btn, 8, 0);
    lv_obj_align(init_btn, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_add_event_cb(init_btn, init_audio, LV_EVENT_CLICKED, NULL);

    lv_obj_t *init_label = lv_label_create(init_btn);
    lv_label_set_text(init_label, "Init Audio");
    lv_obj_set_style_text_font(init_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(init_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(init_label);

    lv_obj_t *vol_label = lv_label_create(scr);
    lv_label_set_text(vol_label, "Volume");
    lv_obj_set_style_text_font(vol_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vol_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(vol_label, LV_ALIGN_LEFT_MID, 40, 0);

    volume_slider = lv_slider_create(scr);
    lv_obj_set_size(volume_slider, 200, 20);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x5a5a8a), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(volume_slider, 0, 0);
    lv_obj_align(volume_slider, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_event_cb(volume_slider, volume_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *play_btn = lv_btn_create(scr);
    lv_obj_set_size(play_btn, 200, 60);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_radius(play_btn, 8, 0);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, 50);
    lv_obj_add_event_cb(play_btn, play_tone, LV_EVENT_CLICKED, NULL);

    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "▶ Play Tone");
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(play_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_label);

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

    ESP_LOGI(TAG, "Audio app created");
    return scr;
}
