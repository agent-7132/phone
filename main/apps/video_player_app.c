#include "video_player_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lv_decoder.h"
#include "bsp/esp32_p4_platform.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "VIDEO_PLAYER";

#define MAX_VIDEOS 20
#define MAX_PATH_LEN 512
#define FRAME_BUFFER_SIZE (480 * 800 * 2)

static lv_obj_t *video_list = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *video_display = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *control_container = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *fullscreen_btn = NULL;

static char video_paths[MAX_VIDEOS][MAX_PATH_LEN];
static int video_count = 0;
static int current_video = -1;
static bool is_playing = false;
static bool is_fullscreen = false;
static TaskHandle_t play_task_handle = NULL;
static FILE *video_file = NULL;
static size_t video_file_size = 0;
static size_t current_position = 0;
static int volume = 50;
static esp_codec_dev_handle_t speaker = NULL;

typedef enum {
    PLAYER_MODE_LIST,
    PLAYER_MODE_PLAYING
} player_mode_t;

static player_mode_t current_mode = PLAYER_MODE_LIST;
static void play_selected_video(lv_event_t *e);

static bool is_video_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".mjpeg") == 0 || strcmp(ext, ".MP4") == 0 ||
            strcmp(ext, ".mp4") == 0 || strcmp(ext, ".AVI") == 0 ||
            strcmp(ext, ".avi") == 0 || strcmp(ext, ".3GP") == 0 ||
            strcmp(ext, ".3gp") == 0 || strcmp(ext, ".mkv") == 0 ||
            strcmp(ext, ".MKV") == 0 || strcmp(ext, ".mov") == 0 ||
            strcmp(ext, ".MOV") == 0 || strcmp(ext, ".wmv") == 0 ||
            strcmp(ext, ".WMV") == 0);
}

static void scan_video_files(void)
{
    video_count = 0;
    memset(video_paths, 0, sizeof(video_paths));
    
    if (!permission_check("VideoPlayer", PERMISSION_STORAGE)) {
        ESP_LOGW(TAG, "Storage permission denied for video scan");
        if (status_label) {
            lv_label_set_text(status_label, "Storage permission denied");
        }
        permission_request("VideoPlayer", PERMISSION_STORAGE);
        return;
    }

    const char *paths[] = {"/sdcard/videos", "/sdcard", "/spiffs/videos", "/spiffs"};
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]) && video_count < MAX_VIDEOS; i++) {
        DIR *dir = opendir(paths[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && video_count < MAX_VIDEOS) {
            if (entry->d_name[0] == '.') continue;
            if (!is_video_file(entry->d_name)) continue;

            snprintf(video_paths[video_count], MAX_PATH_LEN, "%s/%s", paths[i], entry->d_name);
            video_count++;
        }
        closedir(dir);
    }
}

static void update_video_list(void)
{
    if (!video_list) return;

    lv_obj_clean(video_list);

    for (int i = 0; i < video_count; i++) {
        const char *filename = strrchr(video_paths[i], '/');
        filename = filename ? filename + 1 : video_paths[i];

        lv_obj_t *btn = lv_btn_create(video_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, (i == current_video) ? lv_color_hex(0x4a8a6a) : lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, filename);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, (i == current_video) ? lv_color_hex(0x00FF00) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, (lv_event_cb_t)play_selected_video, LV_EVENT_CLICKED, NULL);
    }

    char status[64];
    snprintf(status, sizeof(status), "Videos: %d", video_count);
    lv_label_set_text(status_label, status);
}

static lv_obj_t *video_img = NULL;

static void play_video_task(void *arg)
{
    (void)arg;
    uint8_t *frame_buffer = (uint8_t *)heap_caps_malloc(FRAME_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (!frame_buffer) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        vTaskDelete(NULL);
        return;
    }
    
    if (!video_file) {
        heap_caps_free(frame_buffer);
        vTaskDelete(NULL);
        return;
    }
    
    if (video_img) {
        lv_obj_del(video_img);
        video_img = NULL;
    }
    
    video_img = lv_img_create(video_display);
    lv_obj_center(video_img);
    
    lv_img_dsc_t img_dsc = {
        .header.cf = LV_COLOR_FORMAT_RGB565,
        .header.w = 480,
        .header.h = 800,
        .data_size = FRAME_BUFFER_SIZE,
        .data = frame_buffer
    };
    
    int frame_count = 0;
    uint64_t start_time = esp_timer_get_time();
    
    while (is_playing && video_file) {
        size_t bytes_read = fread(frame_buffer, 1, FRAME_BUFFER_SIZE, video_file);
        
        if (bytes_read == 0) {
            if (feof(video_file)) {
                ESP_LOGI(TAG, "End of video file");
            } else {
                ESP_LOGE(TAG, "Error reading video file");
            }
            break;
        }
        
        current_position = ftell(video_file);
        int progress = (int)((current_position * 100) / video_file_size);
        lv_slider_set_value(progress_bar, progress, LV_ANIM_OFF);
        
        frame_count++;
        uint64_t elapsed = esp_timer_get_time() - start_time;
        float fps = (frame_count * 1000000.0f) / elapsed;
        
        char time_str[64];
        snprintf(time_str, sizeof(time_str), "FPS: %.1f", fps);
        lv_label_set_text(time_label, time_str);
        
        lv_img_set_src(video_img, &img_dsc);
        lv_task_handler();
        
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    heap_caps_free(frame_buffer);
    
    if (video_file) {
        fclose(video_file);
        video_file = NULL;
    }
    
    is_playing = false;
    play_task_handle = NULL;
    lv_label_set_text(status_label, "Playback finished");
    
    vTaskDelete(NULL);
}

static void play_video_by_index(int index)
{
    if (index < 0 || index >= video_count) return;

    if (is_playing && play_task_handle) {
        is_playing = false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    video_file = fopen(video_paths[index], "rb");
    if (!video_file) {
        lv_label_set_text(status_label, "Failed to open video");
        ESP_LOGE(TAG, "Failed to open video: %s", video_paths[index]);
        return;
    }

    fseek(video_file, 0, SEEK_END);
    video_file_size = ftell(video_file);
    fseek(video_file, 0, SEEK_SET);
    current_position = 0;

    current_video = index;
    is_playing = true;

    const char *filename = strrchr(video_paths[current_video], '/');
    filename = filename ? filename + 1 : video_paths[current_video];
    lv_label_set_text(time_label, filename);
    lv_label_set_text(status_label, "Playing...");

    update_video_list();
    lv_slider_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_slider_set_range(progress_bar, 0, 100);

    xTaskCreatePinnedToCore(play_video_task, "video_play", 4096, NULL, 5, &play_task_handle, 1);

    ESP_LOGI(TAG, "Playing video: %s", video_paths[current_video]);
}

static void play_selected_video(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    play_video_by_index(index);
}

static void toggle_play(lv_event_t *e)
{
    (void)e;
    if (current_video < 0 || current_video >= video_count) {
        if (video_count > 0) {
            play_video_by_index(0);
        } else {
            lv_label_set_text(status_label, "No videos found");
        }
        return;
    }

    if (is_playing) {
        is_playing = false;
        lv_label_set_text(status_label, "Paused");
        if (play_task_handle) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    } else {
        play_video_by_index(current_video);
    }

    ESP_LOGI(TAG, "Playback %s", is_playing ? "resumed" : "paused");
}

static void prev_video(lv_event_t *e)
{
    (void)e;
    if (video_count == 0) return;

    current_video = (current_video <= 0) ? video_count - 1 : current_video - 1;
    play_video_by_index(current_video);
}

static void next_video(lv_event_t *e)
{
    (void)e;
    if (video_count == 0) return;

    current_video = (current_video >= video_count - 1) ? 0 : current_video + 1;
    play_video_by_index(current_video);
}

static void seek_video(lv_event_t *e)
{
    if (!video_file || !is_playing) return;
    
    int progress = lv_slider_get_value(progress_bar);
    size_t seek_pos = (video_file_size * progress) / 100;
    
    fseek(video_file, seek_pos, SEEK_SET);
    current_position = seek_pos;
    
    char status[64];
    snprintf(status, sizeof(status), "Seeked to %d%%", progress);
    lv_label_set_text(status_label, status);
}

static void adjust_volume(lv_event_t *e)
{
    volume = lv_slider_get_value(volume_slider);
    if (speaker) {
        esp_codec_dev_set_out_vol(speaker, volume);
    }
    ESP_LOGI(TAG, "Volume set to %d%%", volume);
}

static void toggle_fullscreen(lv_event_t *e)
{
    (void)e;
    is_fullscreen = !is_fullscreen;
    
    if (is_fullscreen) {
        lv_obj_add_flag(video_display, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(video_display, 480, 800);
        lv_obj_align(video_display, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(control_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(fullscreen_btn, "⛶");
        ui_feedback_show_toast("Fullscreen", "Entered fullscreen mode", UI_FEEDBACK_TYPE_INFO, 1500);
    } else {
        lv_obj_remove_flag(video_display, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(video_display, 400, 250);
        lv_obj_align(video_display, LV_ALIGN_TOP_MID, 0, 110);
        lv_obj_remove_flag(control_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(fullscreen_btn, "⛶");
        ui_feedback_show_toast("Fullscreen", "Exited fullscreen mode", UI_FEEDBACK_TYPE_INFO, 1500);
    }
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    is_playing = false;
    if (play_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (video_file) {
        fclose(video_file);
        video_file = NULL;
    }
    app_manager_go_home();
}

static void init_audio(void)
{
    if (speaker) return;
    
    esp_err_t ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    speaker = bsp_audio_codec_speaker_init();
    if (speaker) {
        esp_codec_dev_set_out_vol(speaker, volume);
        ESP_LOGI(TAG, "Audio initialized");
    } else {
        ESP_LOGE(TAG, "Speaker init failed");
    }
}

static lv_obj_t *create_icon_button(lv_obj_t *parent, const char *icon, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 45, 45);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);

    return btn;
}

lv_obj_t *video_player_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "🎬 Video Player");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Scanning...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);

    video_display = lv_obj_create(scr);
    lv_obj_set_size(video_display, 420, 280);
    lv_obj_set_style_bg_color(video_display, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(video_display, 0, 0);
    lv_obj_set_style_border_color(video_display, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(video_display, 12, 0);
    lv_obj_set_style_shadow_width(video_display, 10, 0);
    lv_obj_set_style_shadow_color(video_display, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(video_display, 80, 0);
    lv_obj_align(video_display, LV_ALIGN_TOP_MID, 0, 110);

    lv_obj_t *display_label = lv_label_create(video_display);
    lv_label_set_text(display_label, "Video Display");
    lv_obj_set_style_text_color(display_label, lv_color_hex(0x666666), 0);
    lv_obj_center(display_label);

    progress_bar = lv_slider_create(scr);
    lv_obj_set_size(progress_bar, 400, 12);
    lv_slider_set_range(progress_bar, 0, 100);
    lv_slider_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x4a8a6a), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(progress_bar, 0, 0);
    lv_obj_align(progress_bar, LV_ALIGN_TOP_MID, 0, 380);
    lv_obj_add_event_cb(progress_bar, seek_video, LV_EVENT_VALUE_CHANGED, NULL);

    time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "No video playing");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 400);

    control_container = lv_obj_create(scr);
    lv_obj_set_size(control_container, 440, 70);
    lv_obj_set_style_bg_color(control_container, lv_color_hex(0x2d2d4a), 0);
    lv_obj_set_style_border_width(control_container, 0, 0);
    lv_obj_set_style_radius(control_container, 35, 0);
    lv_obj_set_style_shadow_width(control_container, 8, 0);
    lv_obj_set_style_shadow_color(control_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(control_container, 60, 0);
    lv_obj_align(control_container, LV_ALIGN_TOP_MID, 0, 430);

    lv_obj_t *prev_btn = create_icon_button(control_container, "⏮", prev_video);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 15, 0);

    lv_obj_t *play_btn = lv_btn_create(control_container);
    lv_obj_set_size(play_btn, 55, 55);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_radius(play_btn, 28, 0);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(play_btn, toggle_play, LV_EVENT_CLICKED, NULL);

    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "▶");
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(play_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_label);

    lv_obj_t *next_btn = create_icon_button(control_container, "⏭", next_video);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -140, 0);

    fullscreen_btn = create_icon_button(control_container, "⛶", toggle_fullscreen);
    lv_obj_align(fullscreen_btn, LV_ALIGN_RIGHT_MID, -85, 0);

    lv_obj_t *vol_btn = create_icon_button(control_container, "🔊", NULL);
    lv_obj_align(vol_btn, LV_ALIGN_RIGHT_MID, -30, 0);

    volume_slider = lv_slider_create(control_container);
    lv_obj_set_size(volume_slider, 60, 8);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x4a8a6a), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(volume_slider, 0, 0);
    lv_obj_align(volume_slider, LV_ALIGN_RIGHT_MID, -100, 0);
    lv_obj_add_event_cb(volume_slider, adjust_volume, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 150);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 10, 0);
    lv_obj_align(list_container, LV_ALIGN_BOTTOM_MID, 0, -70);

    video_list = lv_list_create(list_container);
    lv_obj_set_size(video_list, 420, 130);
    lv_obj_align(video_list, LV_ALIGN_CENTER, 0, 0);

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

    init_audio();
    scan_video_files();
    update_video_list();

    ui_animation_fade(scr, true, 300);

    ESP_LOGI(TAG, "Video player app created");
    return scr;
}