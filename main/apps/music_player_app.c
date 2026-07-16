#include "music_player_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "esp_codec_dev.h"
#include "bt_audio_service.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "esp_log.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MUSIC_PLAYER";

#define MAX_SONGS 50
#define MAX_PATH_LEN 512
#define AUDIO_BUFFER_SIZE 512
#define PLAY_TASK_STACK_SIZE 4096
#define PLAY_TASK_PRIORITY 5

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t data_size;
} wav_header_t;

static lv_obj_t *song_list = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *current_song_label = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *album_cover = NULL;
static lv_obj_t *artist_label = NULL;
static lv_obj_t *play_icon = NULL;
static esp_codec_dev_handle_t speaker = NULL;
static FILE *audio_file = NULL;
static TaskHandle_t play_task_handle = NULL;

static char song_paths[MAX_SONGS][MAX_PATH_LEN];
static char song_names[MAX_SONGS][128];
static int song_count = 0;
static int current_song = -1;
static bool is_playing = false;
static int volume = 50;
static bool use_bluetooth_audio = false;

static bool is_music_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".mp3") == 0 || strcmp(ext, ".wav") == 0 ||
            strcmp(ext, ".MP3") == 0 || strcmp(ext, ".WAV") == 0);
}

static void play_selected_song(lv_event_t *e);

static void scan_music_files(void)
{
    song_count = 0;
    memset(song_paths, 0, sizeof(song_paths));
    memset(song_names, 0, sizeof(song_names));
    
    if (!permission_check("MusicPlayer", PERMISSION_STORAGE)) {
        ESP_LOGW(TAG, "Storage permission denied for music scan");
        permission_request("MusicPlayer", PERMISSION_STORAGE);
        return;
    }

    const char *paths[] = {"/sdcard/music", "/sdcard", "/spiffs/music", "/spiffs"};
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]) && song_count < MAX_SONGS; i++) {
        DIR *dir = opendir(paths[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && song_count < MAX_SONGS) {
            if (entry->d_name[0] == '.') continue;
            if (!is_music_file(entry->d_name)) continue;

            snprintf(song_paths[song_count], MAX_PATH_LEN, "%s/%s", paths[i], entry->d_name);
            const char *name = strrchr(entry->d_name, '.');
            if (name) {
                strncpy(song_names[song_count], entry->d_name, name - entry->d_name);
            } else {
                strcpy(song_names[song_count], entry->d_name);
            }
            song_count++;
        }
        closedir(dir);
    }
}

static void update_song_list(void)
{
    if (!song_list) return;

    lv_obj_clean(song_list);

    for (int i = 0; i < song_count; i++) {
        lv_obj_t *btn = lv_btn_create(song_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, 55);
        lv_obj_set_style_bg_color(btn, (i == current_song) ? lv_color_hex(0x4CAF50) : lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);

        lv_obj_t *num_label = lv_label_create(btn);
        char num_str[16];
        snprintf(num_str, sizeof(num_str), "%02d", i + 1);
        lv_label_set_text(num_label, num_str);
        lv_obj_set_style_text_font(num_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(num_label, (i == current_song) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x666666), 0);
        lv_obj_align(num_label, LV_ALIGN_LEFT_MID, 15, 0);

        lv_obj_t *name_label = lv_label_create(btn);
        lv_label_set_text(name_label, song_names[i]);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_label, (i == current_song) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 50, 0);

        lv_obj_t *duration_label = lv_label_create(btn);
        lv_label_set_text(duration_label, "--:--");
        lv_obj_set_style_text_font(duration_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(duration_label, lv_color_hex(0x666666), 0);
        lv_obj_align(duration_label, LV_ALIGN_RIGHT_MID, -15, 0);

        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, (lv_event_cb_t)play_selected_song, LV_EVENT_CLICKED, NULL);
    }

    char status[64];
    snprintf(status, sizeof(status), "%d Songs", song_count);
    lv_label_set_text(status_label, status);
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

static bool parse_wav_header(FILE *file, wav_header_t *header)
{
    char riff[5] = {0};
    char wave[5] = {0};
    char fmt[5] = {0};
    char data[5] = {0};
    uint32_t chunk_size;
    uint32_t fmt_chunk_size;

    if (fread(riff, 1, 4, file) != 4 || strcmp(riff, "RIFF") != 0) {
        ESP_LOGE(TAG, "Not a RIFF file");
        return false;
    }

    if (fread(&chunk_size, 4, 1, file) != 1) return false;

    if (fread(wave, 1, 4, file) != 4 || strcmp(wave, "WAVE") != 0) {
        ESP_LOGE(TAG, "Not a WAVE file");
        return false;
    }

    if (fread(fmt, 1, 4, file) != 4 || strcmp(fmt, "fmt ") != 0) {
        ESP_LOGE(TAG, "Invalid WAVE format");
        return false;
    }

    if (fread(&fmt_chunk_size, 4, 1, file) != 1) return false;

    if (fread(&header->audio_format, 2, 1, file) != 1) return false;
    if (fread(&header->num_channels, 2, 1, file) != 1) return false;
    if (fread(&header->sample_rate, 4, 1, file) != 1) return false;
    if (fread(&header->byte_rate, 4, 1, file) != 1) return false;
    if (fread(&header->block_align, 2, 1, file) != 1) return false;
    if (fread(&header->bits_per_sample, 2, 1, file) != 1) return false;

    if (fmt_chunk_size > 16) {
        fseek(file, fmt_chunk_size - 16, SEEK_CUR);
    }

    if (fread(data, 1, 4, file) != 4 || strcmp(data, "data") != 0) {
        ESP_LOGE(TAG, "Missing data chunk");
        return false;
    }

    if (fread(&header->data_size, 4, 1, file) != 1) return false;

    ESP_LOGI(TAG, "WAV Format: PCM=%d, Channels=%d, SampleRate=%d, Bits=%d",
             header->audio_format, header->num_channels, 
             header->sample_rate, header->bits_per_sample);

    return true;
}

static void play_audio_task(void *arg)
{
    (void)arg;
    uint8_t buffer[AUDIO_BUFFER_SIZE];
    size_t bytes_read;
    wav_header_t wav_header;

    if (!audio_file) {
        vTaskDelete(NULL);
        return;
    }

    if (!parse_wav_header(audio_file, &wav_header)) {
        ESP_LOGE(TAG, "Failed to parse WAV header");
        fclose(audio_file);
        audio_file = NULL;
        vTaskDelete(NULL);
        return;
    }

    while (is_playing && audio_file) {
        bytes_read = fread(buffer, 1, AUDIO_BUFFER_SIZE, audio_file);
        
        if (bytes_read == 0) {
            if (feof(audio_file)) {
                ESP_LOGI(TAG, "End of audio file");
                is_playing = false;
            } else {
                ESP_LOGE(TAG, "Error reading audio file");
            }
            break;
        }

        if (use_bluetooth_audio && bt_audio_service_is_streaming()) {
            bt_audio_service_send_data(buffer, bytes_read);
        } else if (speaker) {
            esp_codec_dev_write(speaker, buffer, bytes_read);
            if (use_bluetooth_audio && !bt_audio_service_is_streaming()) {
                lv_label_set_text(status_label, "BT Audio: No connection");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (audio_file) {
        fclose(audio_file);
        audio_file = NULL;
    }

    is_playing = false;
    play_task_handle = NULL;
    vTaskDelete(NULL);
}

static void play_song_by_index(int index)
{
    if (index < 0 || index >= song_count) return;

    if (is_playing && play_task_handle) {
        is_playing = false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    init_audio();

    audio_file = fopen(song_paths[index], "rb");
    if (!audio_file) {
        lv_label_set_text(status_label, "Failed to open file");
        ui_feedback_show_toast("Playback Error", "Cannot open audio file", UI_FEEDBACK_TYPE_ERROR, 2000);
        ESP_LOGE(TAG, "Failed to open file: %s", song_paths[index]);
        return;
    }

    current_song = index;
    is_playing = true;

    lv_label_set_text(current_song_label, song_names[current_song]);
    lv_label_set_text(artist_label, "Unknown Artist");
    lv_label_set_text(status_label, "Playing...");
    lv_label_set_text(play_icon, "⏸");

    update_song_list();
    lv_slider_set_value(progress_bar, 0, LV_ANIM_OFF);

    xTaskCreate(play_audio_task, "music_play", PLAY_TASK_STACK_SIZE, NULL, PLAY_TASK_PRIORITY, &play_task_handle);

    ESP_LOGI(TAG, "Playing: %s", song_paths[current_song]);
}

static void play_selected_song(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    play_song_by_index(index);
}

static void toggle_play(lv_event_t *e)
{
    (void)e;
    if (current_song < 0 || current_song >= song_count) {
        if (song_count > 0) {
            play_song_by_index(0);
        } else {
            lv_label_set_text(status_label, "No songs found");
        }
        return;
    }

    if (is_playing) {
        is_playing = false;
        lv_label_set_text(status_label, "Paused");
        lv_label_set_text(play_icon, "▶");
        if (play_task_handle) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    } else {
        play_song_by_index(current_song);
    }

    ESP_LOGI(TAG, "Playback %s", is_playing ? "resumed" : "paused");
}

static void prev_song(lv_event_t *e)
{
    (void)e;
    if (song_count == 0) return;

    current_song = (current_song <= 0) ? song_count - 1 : current_song - 1;
    play_song_by_index(current_song);
}

static void next_song(lv_event_t *e)
{
    (void)e;
    if (song_count == 0) return;

    current_song = (current_song >= song_count - 1) ? 0 : current_song + 1;
    play_song_by_index(current_song);
}

static void volume_changed(lv_event_t *e)
{
    volume = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    if (speaker) {
        esp_codec_dev_set_out_vol(speaker, volume);
    }

    char vol_str[32];
    snprintf(vol_str, sizeof(vol_str), "%d%%", volume);
    lv_label_set_text(time_label, vol_str);
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    is_playing = false;
    if (play_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (audio_file) {
        fclose(audio_file);
        audio_file = NULL;
    }
    app_manager_go_home();
}

static void toggle_bluetooth_audio(lv_event_t *e)
{
    (void)e;
    use_bluetooth_audio = !use_bluetooth_audio;
    
    if (use_bluetooth_audio) {
        lv_label_set_text(status_label, "BT Audio: ON");
        ESP_LOGI(TAG, "Bluetooth audio enabled");
    } else {
        lv_label_set_text(status_label, "BT Audio: OFF");
        ESP_LOGI(TAG, "Bluetooth audio disabled");
    }
}

lv_obj_t *music_player_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Music Player");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *album_container = lv_obj_create(scr);
    lv_obj_set_size(album_container, 220, 220);
    lv_obj_set_style_bg_color(album_container, lv_color_hex(0x2d2d4a), 0);
    lv_obj_set_style_border_width(album_container, 0, 0);
    lv_obj_set_style_radius(album_container, 20, 0);
    lv_obj_set_style_shadow_width(album_container, 12, 0);
    lv_obj_set_style_shadow_color(album_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(album_container, 80, 0);
    lv_obj_align(album_container, LV_ALIGN_TOP_MID, 0, 95);
    
    lv_obj_t *album_inner = lv_obj_create(album_container);
    lv_obj_set_size(album_inner, 190, 190);
    lv_obj_set_style_bg_color(album_inner, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_border_width(album_inner, 0, 0);
    lv_obj_set_style_radius(album_inner, 16, 0);
    lv_obj_center(album_inner);
    
    album_cover = lv_label_create(album_inner);
    lv_label_set_text(album_cover, "🎵");
    lv_obj_set_style_text_font(album_cover, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(album_cover, lv_color_hex(0x66BB6A), 0);
    lv_obj_center(album_cover);

    current_song_label = lv_label_create(scr);
    lv_label_set_text(current_song_label, "No song playing");
    lv_obj_set_style_text_font(current_song_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(current_song_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(current_song_label, LV_ALIGN_TOP_MID, 0, 310);

    artist_label = lv_label_create(scr);
    lv_label_set_text(artist_label, "Unknown Artist");
    lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(artist_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(artist_label, LV_ALIGN_TOP_MID, 0, 335);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Scanning...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 360);

    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 220);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 12, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 395);

    song_list = lv_list_create(list_container);
    lv_obj_set_size(song_list, 420, 200);
    lv_obj_align(song_list, LV_ALIGN_CENTER, 0, 0);

    progress_bar = lv_slider_create(scr);
    lv_obj_set_size(progress_bar, 400, 6);
    lv_slider_set_range(progress_bar, 0, 100);
    lv_slider_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x4CAF50), LV_PART_KNOB);
    lv_obj_set_style_border_width(progress_bar, 0, 0);
    lv_obj_set_style_radius(progress_bar, 3, 0);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, -110);

    time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "50%");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(time_label, LV_ALIGN_BOTTOM_RIGHT, -20, -60);

    lv_obj_t *control_container = lv_obj_create(scr);
    lv_obj_set_size(control_container, 340, 80);
    lv_obj_set_style_bg_color(control_container, lv_color_hex(0x2d2d4a), 0);
    lv_obj_set_style_border_width(control_container, 0, 0);
    lv_obj_set_style_radius(control_container, 40, 0);
    lv_obj_set_style_shadow_width(control_container, 10, 0);
    lv_obj_set_style_shadow_color(control_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(control_container, 80, 0);
    lv_obj_align(control_container, LV_ALIGN_BOTTOM_MID, 0, -15);

    lv_obj_t *prev_btn = lv_btn_create(control_container);
    lv_obj_set_size(prev_btn, 55, 55);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(prev_btn, 0, 0);
    lv_obj_set_style_radius(prev_btn, 27, 0);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 25, 0);
    lv_obj_add_event_cb(prev_btn, prev_song, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "⏮");
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_label);

    lv_obj_t *play_btn = lv_btn_create(control_container);
    lv_obj_set_size(play_btn, 65, 65);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_radius(play_btn, 32, 0);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(play_btn, toggle_play, LV_EVENT_CLICKED, NULL);

    play_icon = lv_label_create(play_btn);
    lv_label_set_text(play_icon, "▶");
    lv_obj_set_style_text_font(play_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(play_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_icon);

    lv_obj_t *next_btn = lv_btn_create(control_container);
    lv_obj_set_size(next_btn, 55, 55);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_radius(next_btn, 27, 0);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -25, 0);
    lv_obj_add_event_cb(next_btn, next_song, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "⏭");
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_label);

    lv_obj_t *volume_slider = lv_slider_create(scr);
    lv_obj_set_size(volume_slider, 100, 4);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x4CAF50), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x4CAF50), LV_PART_KNOB);
    lv_obj_set_style_border_width(volume_slider, 0, 0);
    lv_obj_set_style_radius(volume_slider, 2, 0);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_RIGHT, -80, -60);
    lv_obj_add_event_cb(volume_slider, volume_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *vol_icon = lv_label_create(scr);
    lv_label_set_text(vol_icon, "🔊");
    lv_obj_set_style_text_font(vol_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(vol_icon, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(vol_icon, LV_ALIGN_BOTTOM_RIGHT, -185, -60);

    lv_obj_t *bt_btn = lv_btn_create(scr);
    lv_obj_set_size(bt_btn, 45, 45);
    lv_obj_set_style_bg_color(bt_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(bt_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(bt_btn, 0, 0);
    lv_obj_set_style_radius(bt_btn, 22, 0);
    lv_obj_align(bt_btn, LV_ALIGN_BOTTOM_RIGHT, -240, -55);
    lv_obj_add_event_cb(bt_btn, toggle_bluetooth_audio, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bt_label = lv_label_create(bt_btn);
    lv_label_set_text(bt_label, "🔵");
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bt_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(bt_label);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 80, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -55);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "← Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    scan_music_files();
    update_song_list();
    init_audio();
    
    ui_animation_bounce(scr, true, 400);

    ESP_LOGI(TAG, "Music player app created with modern design");
    return scr;
}