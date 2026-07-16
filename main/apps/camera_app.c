#include "camera_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/display.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "CAMERA_APP";

static lv_obj_t *status_label = NULL;
static lv_obj_t *mode_label = NULL;
static lv_obj_t *recording_timer_label = NULL;
static bool camera_running = false;
static bool is_recording = false;
static int photo_count = 0;
static int video_count = 0;
static TaskHandle_t record_task_handle = NULL;
static SemaphoreHandle_t record_mutex = NULL;
static FILE *video_file = NULL;
static time_t record_start_time;
static lv_obj_t *flash_icon = NULL;
static lv_obj_t *hdr_icon = NULL;
static bool flash_on = false;
static bool hdr_on = false;

typedef enum {
    CAM_MODE_PHOTO,
    CAM_MODE_VIDEO
} camera_mode_t;

static camera_mode_t current_mode = CAM_MODE_PHOTO;

static void write_mjpeg_header(FILE *fp)
{
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    fprintf(fp, "Content-Type: multipart/x-mixed-replace;boundary=frame\r\n");
    fprintf(fp, "\r\n");
}

static void write_mjpeg_frame(FILE *fp, const uint8_t *data, size_t size)
{
    fprintf(fp, "--frame\r\n");
    fprintf(fp, "Content-Type: image/jpeg\r\n");
    fprintf(fp, "Content-Length: %zu\r\n", size);
    fprintf(fp, "\r\n");
    fwrite(data, 1, size, fp);
    fprintf(fp, "\r\n");
    fflush(fp);
}

static void record_video_task(void *arg)
{
    (void)arg;
    uint8_t fake_frame[512] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00, 0x01, 0x01, 0x00, 0x00, 0x01};
    
    while (is_recording && video_file) {
        if (xSemaphoreTake(record_mutex, portMAX_DELAY) == pdTRUE) {
            write_mjpeg_frame(video_file, fake_frame, sizeof(fake_frame));
            xSemaphoreGive(record_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(33));
    }
    
    if (video_file) {
        fclose(video_file);
        video_file = NULL;
    }
    
    record_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_recording(void)
{
    if (is_recording) return;
    
    if (!permission_check("Camera", PERMISSION_CAMERA)) {
        lv_label_set_text(status_label, "Camera permission denied");
        ESP_LOGW(TAG, "Camera permission denied");
        permission_request("Camera", PERMISSION_CAMERA);
        return;
    }
    
    if (!permission_check("Camera", PERMISSION_STORAGE)) {
        lv_label_set_text(status_label, "Storage permission denied");
        ESP_LOGW(TAG, "Storage permission denied");
        permission_request("Camera", PERMISSION_STORAGE);
        return;
    }
    
    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/video_%04d.mjpeg", video_count++);
    
    video_file = fopen(filename, "wb");
    if (!video_file) {
        snprintf(filename, sizeof(filename), "/spiffs/video_%04d.mjpeg", video_count - 1);
        video_file = fopen(filename, "wb");
    }
    
    if (!video_file) {
        lv_label_set_text(status_label, "Record failed - mount SD card");
        ESP_LOGE(TAG, "Failed to open video file");
        return;
    }
    
    write_mjpeg_header(video_file);
    
    is_recording = true;
    record_start_time = time(NULL);
    
    lv_label_set_text(status_label, "Recording...");
    
    xTaskCreate(record_video_task, "video_record", 2048, NULL, 5, &record_task_handle);
    
    ESP_LOGI(TAG, "Video recording started: %s", filename);
}

static void stop_recording(void)
{
    if (!is_recording) return;
    
    is_recording = false;
    
    if (record_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    char msg[128];
    time_t duration = time(NULL) - record_start_time;
    snprintf(msg, sizeof(msg), "Video saved: %ld sec", (long)duration);
    lv_label_set_text(status_label, msg);
    
    ESP_LOGI(TAG, "Video recording stopped");
}

static void toggle_recording(lv_event_t *e)
{
    (void)e;
    
    if (is_recording) {
        stop_recording();
    } else {
        start_recording();
    }
}

static void capture_photo(lv_event_t *e)
{
    (void)e;
    
    if (is_recording) {
        lv_label_set_text(status_label, "Cannot take photo while recording");
        return;
    }
    
    if (!permission_check("Camera", PERMISSION_CAMERA)) {
        lv_label_set_text(status_label, "Camera permission denied");
        ESP_LOGW(TAG, "Camera permission denied");
        permission_request("Camera", PERMISSION_CAMERA);
        return;
    }
    
    if (!permission_check("Camera", PERMISSION_STORAGE)) {
        lv_label_set_text(status_label, "Storage permission denied");
        ESP_LOGW(TAG, "Storage permission denied");
        permission_request("Camera", PERMISSION_STORAGE);
        return;
    }
    
    char filename[64];
    snprintf(filename, sizeof(filename), "/sdcard/photo_%04d.jpg", photo_count++);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        snprintf(filename, sizeof(filename), "/spiffs/photo_%04d.jpg", photo_count - 1);
        fp = fopen(filename, "wb");
    }
    
    if (fp) {
        uint8_t fake_jpeg[128] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00, 0x01, 0x01, 0x00, 0x00, 0x01};
        fwrite(fake_jpeg, 1, sizeof(fake_jpeg), fp);
        fclose(fp);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "Photo saved: %s", filename);
        lv_label_set_text(status_label, msg);
        ui_feedback_show_toast("Capture Success", msg, UI_FEEDBACK_TYPE_SUCCESS, 2000);
        ESP_LOGI(TAG, "Photo saved: %s", filename);
    } else {
        lv_label_set_text(status_label, "Save failed - mount SD card");
        ui_feedback_show_toast("Capture Failed", "Cannot save photo", UI_FEEDBACK_TYPE_ERROR, 2000);
        ESP_LOGE(TAG, "Failed to save photo");
    }
}

static void toggle_mode(lv_event_t *e)
{
    (void)e;
    
    if (is_recording) {
        stop_recording();
    }
    
    current_mode = (current_mode == CAM_MODE_PHOTO) ? CAM_MODE_VIDEO : CAM_MODE_PHOTO;
    
    if (current_mode == CAM_MODE_PHOTO) {
        lv_label_set_text(mode_label, "Photo");
        lv_label_set_text(status_label, "Ready to capture");
    } else {
        lv_label_set_text(mode_label, "Video");
        lv_label_set_text(status_label, "Ready to record");
    }
    
    ESP_LOGI(TAG, "Switched to %s mode", current_mode == CAM_MODE_PHOTO ? "photo" : "video");
}

static void toggle_flash(lv_event_t *e)
{
    (void)e;
    flash_on = !flash_on;
    lv_label_set_text(flash_icon, flash_on ? "⚡" : "⚡");
    lv_obj_set_style_text_color(flash_icon, flash_on ? lv_color_hex(0xFFD700) : lv_color_hex(0x888888), 0);
    lv_label_set_text(status_label, flash_on ? "Flash: ON" : "Flash: OFF");
}

static void toggle_hdr(lv_event_t *e)
{
    (void)e;
    hdr_on = !hdr_on;
    lv_label_set_text(hdr_icon, hdr_on ? "HDR" : "HDR");
    lv_obj_set_style_text_color(hdr_icon, hdr_on ? lv_color_hex(0x4CAF50) : lv_color_hex(0x888888), 0);
    lv_label_set_text(status_label, hdr_on ? "HDR: ON" : "HDR: OFF");
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    
    if (is_recording) {
        stop_recording();
    }
    
    camera_running = false;
    app_manager_go_home();
}

static void update_timer(lv_timer_t *timer)
{
    (void)timer;
    
    if (is_recording) {
        time_t elapsed = time(NULL) - record_start_time;
        char timer_str[32];
        snprintf(timer_str, sizeof(timer_str), "%02ld:%02ld:%02ld", 
                 (long)(elapsed / 3600), (long)((elapsed % 3600) / 60), (long)(elapsed % 60));
        lv_label_set_text(recording_timer_label, timer_str);
    } else {
        lv_label_set_text(recording_timer_label, "--:--:--");
    }
}

lv_obj_t *camera_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    
    status_bar_create(scr);
    
    lv_obj_t *top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, 480, 65);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(top_bar, 220, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_shadow_width(top_bar, 8, 0);
    lv_obj_set_style_shadow_color(top_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(top_bar, 100, 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 35);
    
    mode_label = lv_label_create(top_bar);
    lv_label_set_text(mode_label, "Photo");
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(mode_label, LV_ALIGN_LEFT_MID, 20, 0);
    
    recording_timer_label = lv_label_create(top_bar);
    lv_label_set_text(recording_timer_label, "--:--:--");
    lv_obj_set_style_text_font(recording_timer_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(recording_timer_label, lv_color_hex(0xFF4444), 0);
    lv_obj_align(recording_timer_label, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t *right_container = lv_obj_create(top_bar);
    lv_obj_set_size(right_container, 100, 40);
    lv_obj_set_style_bg_color(right_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(right_container, 0, 0);
    lv_obj_align(right_container, LV_ALIGN_RIGHT_MID, -10, 0);
    
    flash_icon = lv_label_create(right_container);
    lv_label_set_text(flash_icon, "⚡");
    lv_obj_set_style_text_font(flash_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(flash_icon, lv_color_hex(0x888888), 0);
    lv_obj_align(flash_icon, LV_ALIGN_LEFT_MID, 0, 0);
    
    hdr_icon = lv_label_create(right_container);
    lv_label_set_text(hdr_icon, "HDR");
    lv_obj_set_style_text_font(hdr_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hdr_icon, lv_color_hex(0x888888), 0);
    lv_obj_align(hdr_icon, LV_ALIGN_RIGHT_MID, 0, 0);
    
    lv_obj_t *flash_btn = lv_btn_create(top_bar);
    lv_obj_set_size(flash_btn, 40, 40);
    lv_obj_set_style_bg_color(flash_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(flash_btn, 0, 0);
    lv_obj_set_style_border_width(flash_btn, 0, 0);
    lv_obj_align(flash_btn, LV_ALIGN_RIGHT_MID, -60, 0);
    lv_obj_add_event_cb(flash_btn, toggle_flash, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *hdr_btn = lv_btn_create(top_bar);
    lv_obj_set_size(hdr_btn, 40, 40);
    lv_obj_set_style_bg_color(hdr_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hdr_btn, 0, 0);
    lv_obj_set_style_border_width(hdr_btn, 0, 0);
    lv_obj_align(hdr_btn, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_add_event_cb(hdr_btn, toggle_hdr, LV_EVENT_CLICKED, NULL);
    
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Ready to capture");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(status_label, 80, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 105);
    
    lv_obj_t *preview_frame = lv_obj_create(scr);
    lv_obj_set_size(preview_frame, 460, 520);
    lv_obj_set_style_bg_color(preview_frame, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_border_width(preview_frame, 0, LV_PART_MAIN);
    lv_obj_align(preview_frame, LV_ALIGN_CENTER, 0, -20);
    
    lv_obj_t *focus_frame = lv_obj_create(preview_frame);
    lv_obj_set_size(focus_frame, 80, 80);
    lv_obj_set_style_bg_color(focus_frame, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(focus_frame, 0, 0);
    lv_obj_set_style_border_width(focus_frame, 2, 0);
    lv_obj_set_style_border_color(focus_frame, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_opa(focus_frame, 80, 0);
    lv_obj_align(focus_frame, LV_ALIGN_CENTER, 0, 0);
    
    lv_obj_t *focus_corner_tl = lv_obj_create(focus_frame);
    lv_obj_set_size(focus_corner_tl, 12, 12);
    lv_obj_set_style_bg_color(focus_corner_tl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(focus_corner_tl, 0, 0);
    lv_obj_set_style_radius(focus_corner_tl, 2, 0);
    lv_obj_align(focus_corner_tl, LV_ALIGN_TOP_LEFT, -2, -2);
    
    lv_obj_t *focus_corner_tr = lv_obj_create(focus_frame);
    lv_obj_set_size(focus_corner_tr, 12, 12);
    lv_obj_set_style_bg_color(focus_corner_tr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(focus_corner_tr, 0, 0);
    lv_obj_set_style_radius(focus_corner_tr, 2, 0);
    lv_obj_align(focus_corner_tr, LV_ALIGN_TOP_RIGHT, 2, -2);
    
    lv_obj_t *focus_corner_bl = lv_obj_create(focus_frame);
    lv_obj_set_size(focus_corner_bl, 12, 12);
    lv_obj_set_style_bg_color(focus_corner_bl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(focus_corner_bl, 0, 0);
    lv_obj_set_style_radius(focus_corner_bl, 2, 0);
    lv_obj_align(focus_corner_bl, LV_ALIGN_BOTTOM_LEFT, -2, 2);
    
    lv_obj_t *focus_corner_br = lv_obj_create(focus_frame);
    lv_obj_set_size(focus_corner_br, 12, 12);
    lv_obj_set_style_bg_color(focus_corner_br, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(focus_corner_br, 0, 0);
    lv_obj_set_style_radius(focus_corner_br, 2, 0);
    lv_obj_align(focus_corner_br, LV_ALIGN_BOTTOM_RIGHT, 2, 2);
    
    lv_obj_t *preview_label = lv_label_create(preview_frame);
    lv_label_set_text(preview_label, "Preview Area\n(Camera feed)");
    lv_obj_set_style_text_font(preview_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(preview_label, lv_color_hex(0x555555), 0);
    lv_obj_center(preview_label);
    
    lv_obj_t *bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, 480, 110);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_opa(bottom_bar, 220, 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 20, 0);
    lv_obj_set_style_shadow_width(bottom_bar, 12, 0);
    lv_obj_set_style_shadow_color(bottom_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(bottom_bar, 100, 0);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    lv_obj_t *mode_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(mode_btn, 70, 70);
    lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x4a4a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(mode_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(mode_btn, 35, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(mode_btn, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(mode_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(mode_btn, 60, LV_PART_MAIN);
    lv_obj_align(mode_btn, LV_ALIGN_BOTTOM_LEFT, 30, -20);
    lv_obj_add_event_cb(mode_btn, toggle_mode, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *mode_icon = lv_label_create(mode_btn);
    lv_label_set_text(mode_icon, "📷");
    lv_obj_set_style_text_font(mode_icon, &lv_font_montserrat_14, 0);
    lv_obj_center(mode_icon);
    
    lv_obj_t *capture_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(capture_btn, 95, 95);
    lv_obj_set_style_bg_color(capture_btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(capture_btn, lv_color_hex(0xE0E0E0), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(capture_btn, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(capture_btn, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_radius(capture_btn, 47, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(capture_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(capture_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(capture_btn, 60, LV_PART_MAIN);
    lv_obj_align(capture_btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    if (current_mode == CAM_MODE_PHOTO) {
        lv_obj_add_event_cb(capture_btn, capture_photo, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_add_event_cb(capture_btn, toggle_recording, LV_EVENT_CLICKED, NULL);
    }
    
    lv_obj_t *capture_inner = lv_obj_create(capture_btn);
    lv_obj_set_size(capture_inner, 65, 65);
    lv_obj_set_style_bg_color(capture_inner, current_mode == CAM_MODE_VIDEO ? lv_color_hex(0xFF4444) : lv_color_hex(0xFF4444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(capture_inner, current_mode == CAM_MODE_VIDEO ? lv_color_hex(0xFF3333) : lv_color_hex(0xFF3333), LV_STATE_PRESSED);
    lv_obj_set_style_radius(capture_inner, current_mode == CAM_MODE_VIDEO ? 8 : 32, LV_PART_MAIN);
    lv_obj_set_style_border_width(capture_inner, 0, LV_PART_MAIN);
    lv_obj_center(capture_inner);
    
    lv_obj_t *gallery_btn = lv_btn_create(bottom_bar);
    lv_obj_set_size(gallery_btn, 70, 70);
    lv_obj_set_style_bg_color(gallery_btn, lv_color_hex(0x4a4a6a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(gallery_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(gallery_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(gallery_btn, 35, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(gallery_btn, 6, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(gallery_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(gallery_btn, 60, LV_PART_MAIN);
    lv_obj_align(gallery_btn, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
    
    lv_obj_t *gallery_icon = lv_label_create(gallery_btn);
    lv_label_set_text(gallery_icon, "🖼️");
    lv_obj_set_style_text_font(gallery_icon, &lv_font_montserrat_14, 0);
    lv_obj_center(gallery_icon);
    
    record_mutex = xSemaphoreCreateMutex();
    
    lv_timer_create(update_timer, 1000, NULL);
    
    camera_running = false;
    
    ui_animation_scale(scr, true, 300);
    
    ESP_LOGI(TAG, "Camera app created");
    return scr;
}