#include "photo_album_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "gesture_manager.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "esp_log.h"
#include "esp_jpeg_dec.h"
#include "esp_jpeg_common.h"
#include "esp_check.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "PHOTO_ALBUM";

#define MAX_PHOTOS 100
#define MAX_PATH_LEN 512

static lv_obj_t *photo_grid = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *preview_container = NULL;
static lv_obj_t *preview_img = NULL;
static lv_obj_t *preview_close_btn = NULL;
static lv_obj_t *preview_info_bar = NULL;

static char photo_paths[MAX_PHOTOS][MAX_PATH_LEN];
static int photo_count = 0;
static int current_photo = -1;
static int zoom_level = 1;

static bool is_image_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".png") == 0 || strcmp(ext, ".bmp") == 0 ||
            strcmp(ext, ".JPG") == 0 || strcmp(ext, ".JPEG") == 0 ||
            strcmp(ext, ".PNG") == 0 || strcmp(ext, ".BMP") == 0);
}

static bool is_jpeg_file(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 ||
            strcmp(ext, ".JPG") == 0 || strcmp(ext, ".JPEG") == 0);
}

static void scan_photos(void)
{
    photo_count = 0;
    memset(photo_paths, 0, sizeof(photo_paths));
    
    if (!permission_check("PhotoAlbum", PERMISSION_STORAGE)) {
        ESP_LOGW(TAG, "Storage permission denied for photo scan");
        if (status_label) {
            lv_label_set_text(status_label, "Storage permission denied");
        }
        permission_request("PhotoAlbum", PERMISSION_STORAGE);
        return;
    }

    const char *paths[] = {"/sdcard/photos", "/sdcard", "/spiffs/photos", "/spiffs"};
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]) && photo_count < MAX_PHOTOS; i++) {
        DIR *dir = opendir(paths[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && photo_count < MAX_PHOTOS) {
            if (entry->d_name[0] == '.') continue;
            if (!is_image_file(entry->d_name)) continue;

            snprintf(photo_paths[photo_count], MAX_PATH_LEN, "%s/%s", paths[i], entry->d_name);
            photo_count++;
        }
        closedir(dir);
    }
}

static lv_img_dsc_t *jpeg_decode_hw(const char *file_path)
{
    if (!file_path || !is_jpeg_file(file_path)) {
        return NULL;
    }

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *jpeg_buf = (uint8_t *)malloc(file_size);
    if (!jpeg_buf) {
        fclose(fp);
        ESP_LOGE(TAG, "Failed to allocate JPEG buffer");
        return NULL;
    }

    if (fread(jpeg_buf, 1, file_size, fp) != file_size) {
        fclose(fp);
        free(jpeg_buf);
        ESP_LOGE(TAG, "Failed to read file: %s", file_path);
        return NULL;
    }
    fclose(fp);

    jpeg_dec_config_t dec_config = DEFAULT_JPEG_DEC_CONFIG();
    dec_config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;

    jpeg_dec_handle_t dec_handle = NULL;
    jpeg_error_t ret = jpeg_dec_open(&dec_config, &dec_handle);
    if (ret != JPEG_ERR_OK) {
        free(jpeg_buf);
        ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", ret);
        return NULL;
    }

    jpeg_dec_io_t dec_io = {
        .inbuf = jpeg_buf,
        .inbuf_len = file_size,
        .inbuf_remain = file_size,
    };

    jpeg_dec_header_info_t header_info = {0};
    ret = jpeg_dec_parse_header(dec_handle, &dec_io, &header_info);
    if (ret != JPEG_ERR_OK) {
        jpeg_dec_close(dec_handle);
        free(jpeg_buf);
        ESP_LOGE(TAG, "Failed to parse JPEG header: %d", ret);
        return NULL;
    }

    int outbuf_len = 0;
    ret = jpeg_dec_get_outbuf_len(dec_handle, &outbuf_len);
    if (ret != JPEG_ERR_OK) {
        jpeg_dec_close(dec_handle);
        free(jpeg_buf);
        ESP_LOGE(TAG, "Failed to get output buffer length: %d", ret);
        return NULL;
    }

    uint8_t *outbuf = (uint8_t *)jpeg_calloc_align(outbuf_len, 16);
    if (!outbuf) {
        jpeg_dec_close(dec_handle);
        free(jpeg_buf);
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        return NULL;
    }

    dec_io.outbuf = outbuf;
    ret = jpeg_dec_process(dec_handle, &dec_io);
    jpeg_dec_close(dec_handle);
    free(jpeg_buf);

    if (ret != JPEG_ERR_OK) {
        free(outbuf);
        ESP_LOGE(TAG, "JPEG decode failed: %d", ret);
        return NULL;
    }

    lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)malloc(sizeof(lv_img_dsc_t));
    if (!img_dsc) {
        free(outbuf);
        ESP_LOGE(TAG, "Failed to allocate image descriptor");
        return NULL;
    }

    img_dsc->data = (const void *)outbuf;
    img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc->header.flags = 0;
    img_dsc->header.w = header_info.width;
    img_dsc->header.h = header_info.height;

    ESP_LOGI(TAG, "JPEG decoded: %dx%d, size: %d bytes", 
             header_info.width, header_info.height, outbuf_len);

    return img_dsc;
}

static void update_zoom(void)
{
    if (!preview_img) return;
    
    int base_w = 460;
    int base_h = 600;
    int new_w = base_w * zoom_level;
    int new_h = base_h * zoom_level;
    
    lv_obj_set_size(preview_img, new_w, new_h);
    lv_obj_align(preview_img, LV_ALIGN_CENTER, 0, 0);
}

static void show_preview(int index)
{
    if (index < 0 || index >= photo_count) return;

    current_photo = index;
    zoom_level = 1;

    lv_obj_clear_flag(preview_container, LV_OBJ_FLAG_HIDDEN);

    if (is_jpeg_file(photo_paths[index])) {
        lv_img_dsc_t *img_dsc = jpeg_decode_hw(photo_paths[index]);
        if (img_dsc) {
            lv_img_set_src(preview_img, img_dsc);
            lv_obj_set_user_data(preview_img, img_dsc);
        } else {
            lv_img_set_src(preview_img, photo_paths[index]);
            lv_obj_set_user_data(preview_img, NULL);
        }
    } else {
        lv_img_set_src(preview_img, photo_paths[index]);
        lv_obj_set_user_data(preview_img, NULL);
    }
    update_zoom();

    const char *filename = strrchr(photo_paths[index], '/');
    filename = filename ? filename + 1 : photo_paths[index];
    char status[512];
    snprintf(status, sizeof(status), "%.200s (%d/%d) - Zoom: %dx", filename, index + 1, photo_count, zoom_level);
    lv_label_set_text(status_label, status);

    ESP_LOGI(TAG, "Previewing: %s", photo_paths[index]);
}

static void hide_preview(lv_event_t *e)
{
    (void)e;
    
    lv_img_dsc_t *img_dsc = (lv_img_dsc_t *)lv_obj_get_user_data(preview_img);
    if (img_dsc) {
        free((void *)img_dsc->data);
        free(img_dsc);
        lv_obj_set_user_data(preview_img, NULL);
    }
    
    lv_obj_add_flag(preview_container, LV_OBJ_FLAG_HIDDEN);
    current_photo = -1;
    zoom_level = 1;
    char status[64];
    snprintf(status, sizeof(status), "Photos: %d", photo_count);
    lv_label_set_text(status_label, status);
}

static void photo_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    if (!btn) return;

    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    show_preview(index);
}

static void prev_photo(lv_event_t *e)
{
    (void)e;
    if (photo_count == 0) return;

    lv_img_dsc_t *old_dsc = (lv_img_dsc_t *)lv_obj_get_user_data(preview_img);
    if (old_dsc) {
        free((void *)old_dsc->data);
        free(old_dsc);
        lv_obj_set_user_data(preview_img, NULL);
    }

    int new_index = (current_photo <= 0) ? photo_count - 1 : current_photo - 1;
    show_preview(new_index);
}

static void next_photo(lv_event_t *e)
{
    (void)e;
    if (photo_count == 0) return;

    lv_img_dsc_t *old_dsc = (lv_img_dsc_t *)lv_obj_get_user_data(preview_img);
    if (old_dsc) {
        free((void *)old_dsc->data);
        free(old_dsc);
        lv_obj_set_user_data(preview_img, NULL);
    }

    int new_index = (current_photo >= photo_count - 1) ? 0 : current_photo + 1;
    show_preview(new_index);
}

static void gesture_handler(gesture_type_t type, lv_point_t start, lv_point_t end)
{
    if (current_photo < 0) return;

    switch (type) {
        case GESTURE_TYPE_DOUBLE_TAP:
            zoom_level = (zoom_level >= 3) ? 1 : zoom_level + 1;
            update_zoom();
            {
                const char *filename = strrchr(photo_paths[current_photo], '/');
                filename = filename ? filename + 1 : photo_paths[current_photo];
                char status[512];
                snprintf(status, sizeof(status), "%.200s (%d/%d) - Zoom: %dx", filename, current_photo + 1, photo_count, zoom_level);
                lv_label_set_text(status_label, status);
            }
            ESP_LOGI(TAG, "Double tap - zoom level: %d", zoom_level);
            break;
            
        case GESTURE_TYPE_SWIPE_LEFT:
            if (photo_count > 1) {
                lv_img_dsc_t *old_dsc = (lv_img_dsc_t *)lv_obj_get_user_data(preview_img);
                if (old_dsc) {
                    free((void *)old_dsc->data);
                    free(old_dsc);
                    lv_obj_set_user_data(preview_img, NULL);
                }
                
                int new_index = (current_photo >= photo_count - 1) ? 0 : current_photo + 1;
                show_preview(new_index);
                ESP_LOGI(TAG, "Swipe left - next photo: %d", new_index);
            }
            break;
            
        case GESTURE_TYPE_SWIPE_RIGHT:
            if (photo_count > 1) {
                lv_img_dsc_t *old_dsc = (lv_img_dsc_t *)lv_obj_get_user_data(preview_img);
                if (old_dsc) {
                    free((void *)old_dsc->data);
                    free(old_dsc);
                    lv_obj_set_user_data(preview_img, NULL);
                }
                
                int new_index = (current_photo <= 0) ? photo_count - 1 : current_photo - 1;
                show_preview(new_index);
                ESP_LOGI(TAG, "Swipe right - prev photo: %d", new_index);
            }
            break;
            
        case GESTURE_TYPE_SWIPE_DOWN:
            hide_preview(NULL);
            ESP_LOGI(TAG, "Swipe down - close preview");
            break;
            
        default:
            break;
    }
}

static void update_photo_grid(void)
{
    if (!photo_grid) return;

    lv_obj_clean(photo_grid);

    int cols = 3;
    int rows = (photo_count + cols - 1) / cols;
    int cell_w = 140;
    int cell_h = 130;

    for (int i = 0; i < photo_count; i++) {
        int col = i % cols;
        int row = i / cols;

        lv_obj_t *card = lv_obj_create(photo_grid);
        lv_obj_set_size(card, cell_w - 8, cell_h - 8);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x252540), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_shadow_width(card, 6, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(card, 50, 0);
        lv_obj_set_pos(card, col * cell_w + 4, row * cell_h + 4);

        lv_obj_t *img = lv_img_create(card);
        lv_img_set_src(img, photo_paths[i]);
        lv_obj_set_size(img, cell_w - 18, cell_h - 45);
        lv_obj_set_style_radius(img, 8, 0);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 5);

        const char *filename = strrchr(photo_paths[i], '/');
        filename = filename ? filename + 1 : photo_paths[i];

        lv_obj_t *label = lv_label_create(card);
        lv_label_set_text(label, filename);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_size(label, cell_w - 20, 20);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -5);

        lv_obj_set_user_data(card, (void *)(intptr_t)i);
        lv_obj_add_event_cb(card, photo_click_cb, LV_EVENT_CLICKED, NULL);
    }

    char status[64];
    snprintf(status, sizeof(status), "Photos: %d", photo_count);
    lv_label_set_text(status_label, status);
}

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    hide_preview(e);
    gesture_manager_unregister_callback(gesture_handler);
    app_manager_go_home();
}

lv_obj_t *photo_album_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Photo Album");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *instruction = lv_label_create(scr);
    lv_label_set_text(instruction, "Double-tap to zoom, swipe to navigate");
    lv_obj_set_style_text_font(instruction, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(instruction, lv_color_hex(0x666666), 0);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 75);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Scanning...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *grid_container = lv_obj_create(scr);
    lv_obj_set_size(grid_container, 450, 520);
    lv_obj_set_style_bg_color(grid_container, lv_color_hex(0x202038), 0);
    lv_obj_set_style_border_width(grid_container, 0, 0);
    lv_obj_set_style_radius(grid_container, 16, 0);
    lv_obj_set_style_shadow_width(grid_container, 8, 0);
    lv_obj_set_style_shadow_color(grid_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(grid_container, 60, 0);
    lv_obj_align(grid_container, LV_ALIGN_TOP_MID, 0, 130);

    photo_grid = lv_obj_create(grid_container);
    lv_obj_set_size(photo_grid, 430, 500);
    lv_obj_set_style_bg_color(photo_grid, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(photo_grid, 0, 0);
    lv_obj_set_style_border_width(photo_grid, 0, 0);
    lv_obj_align(photo_grid, LV_ALIGN_CENTER, 0, 0);

    preview_container = lv_obj_create(scr);
    lv_obj_set_size(preview_container, 480, 800);
    lv_obj_set_style_bg_color(preview_container, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_width(preview_container, 0, 0);
    lv_obj_add_flag(preview_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(preview_container, LV_ALIGN_CENTER, 0, 0);

    preview_img = lv_img_create(preview_container);
    lv_obj_set_size(preview_img, 460, 600);
    lv_obj_set_style_radius(preview_img, 8, 0);
    lv_obj_align(preview_img, LV_ALIGN_CENTER, 0, 0);

    preview_close_btn = lv_btn_create(preview_container);
    lv_obj_set_size(preview_close_btn, 50, 50);
    lv_obj_set_style_bg_color(preview_close_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(preview_close_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(preview_close_btn, 0, 0);
    lv_obj_set_style_radius(preview_close_btn, 25, 0);
    lv_obj_set_style_shadow_width(preview_close_btn, 6, 0);
    lv_obj_set_style_shadow_color(preview_close_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(preview_close_btn, 60, 0);
    lv_obj_align(preview_close_btn, LV_ALIGN_TOP_RIGHT, -15, 65);
    lv_obj_add_event_cb(preview_close_btn, hide_preview, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_label = lv_label_create(preview_close_btn);
    lv_label_set_text(close_label, "✕");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(close_label);

    lv_obj_t *prev_btn = lv_btn_create(preview_container);
    lv_obj_set_size(prev_btn, 60, 60);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x252540), 0);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(prev_btn, 0, 0);
    lv_obj_set_style_radius(prev_btn, 30, 0);
    lv_obj_set_style_shadow_width(prev_btn, 6, 0);
    lv_obj_set_style_shadow_color(prev_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(prev_btn, 50, 0);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 15, 0);
    lv_obj_add_event_cb(prev_btn, prev_photo, LV_EVENT_CLICKED, NULL);

    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "◀");
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_label);

    lv_obj_t *next_btn = lv_btn_create(preview_container);
    lv_obj_set_size(next_btn, 60, 60);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x252540), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_radius(next_btn, 30, 0);
    lv_obj_set_style_shadow_width(next_btn, 6, 0);
    lv_obj_set_style_shadow_color(next_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(next_btn, 50, 0);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_add_event_cb(next_btn, next_photo, LV_EVENT_CLICKED, NULL);

    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "▶");
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_label);

    preview_info_bar = lv_obj_create(preview_container);
    lv_obj_set_size(preview_info_bar, 400, 45);
    lv_obj_set_style_bg_color(preview_info_bar, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_opa(preview_info_bar, 200, 0);
    lv_obj_set_style_border_width(preview_info_bar, 0, 0);
    lv_obj_set_style_radius(preview_info_bar, 22, 0);
    lv_obj_align(preview_info_bar, LV_ALIGN_BOTTOM_MID, 0, -80);

    lv_obj_t *preview_status_label = lv_label_create(preview_info_bar);
    lv_label_set_text(preview_status_label, "");
    lv_obj_set_style_text_font(preview_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(preview_status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(preview_status_label);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 90, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_set_style_shadow_width(back_btn, 4, 0);
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, 40, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "← Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    gesture_manager_register_callback(gesture_handler);

    scan_photos();
    update_photo_grid();

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    ESP_LOGI(TAG, "Photo album app created with hardware JPEG decode");
    return scr;
}