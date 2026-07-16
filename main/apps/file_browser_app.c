#include "file_browser_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "app_installer.h"
#include "input_tool.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "dirent.h"
#include "sys/stat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "FILE_BROWSER";

static lv_obj_t *list;
static lv_obj_t *path_label;
static lv_obj_t *search_input;
static lv_obj_t *sort_btn;
static lv_obj_t *deep_search_cb;
static lv_obj_t *current_dialog_rename_input;
static lv_obj_t *current_dialog;
static input_tool_t *input_tool;
static char current_path[512] = "/spiffs";
static char search_query[128] = "";
static char *paths[100];
static int path_depth = 0;

#define MAX_FILE_ENTRIES 200
#define MAX_RECURSION_DEPTH 10

typedef enum {
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_DATE
} sort_mode_t;

static sort_mode_t current_sort_mode = SORT_BY_NAME;
static bool is_deep_search = false;

typedef struct {
    char name[256];
    char path[512];
    int is_dir;
    size_t size;
    time_t mtime;
} file_entry_t;

static file_entry_t file_entries[MAX_FILE_ENTRIES];
static int file_entry_count = 0;

static void scan_directory(void);
static void search_files(void);
static void add_file_item(file_entry_t *entry);
static void free_paths(void);
static void clear_file_list(void);
static int compare_files(const void *a, const void *b);
static void open_dir_cb(lv_event_t *e);
static void install_app_cb(lv_event_t *e);
static void file_click_cb(lv_event_t *e);

static void back_button_cb(lv_event_t *e)
{
    free_paths();
    app_manager_go_home();
}

static void free_paths(void)
{
    for (int i = 0; i < path_depth; i++) {
        if (paths[i]) {
            heap_caps_free(paths[i]);
            paths[i] = NULL;
        }
    }
    path_depth = 0;
}

static void up_button_cb(lv_event_t *e)
{
    (void)e;
    if (path_depth > 0) {
        path_depth--;
        strcpy(current_path, paths[path_depth]);
        lv_label_set_text(path_label, current_path);
        clear_file_list();
        scan_directory();
    }
}

static void clear_file_list(void)
{
    uint32_t child_count = lv_obj_get_child_count(list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(list, i);
        if (child) {
            void *user_data = lv_obj_get_user_data(child);
            if (user_data) {
                heap_caps_free(user_data);
                lv_obj_set_user_data(child, NULL);
            }
        }
    }
    lv_obj_clean(list);
    file_entry_count = 0;
}

static void add_file_item(file_entry_t *entry)
{
    lv_obj_t *card = lv_obj_create(list);
    lv_obj_set_size(card, 420, 70);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2d2d4a), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x3d3d5a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 4, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(card, 40, 0);

    lv_obj_t *icon_bg = lv_obj_create(card);
    lv_obj_set_size(icon_bg, 45, 45);
    lv_obj_set_style_bg_color(icon_bg, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(icon_bg, 0, 0);
    lv_obj_set_style_radius(icon_bg, 10, 0);
    lv_obj_align(icon_bg, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *icon_label = lv_label_create(icon_bg);
    if (entry->is_dir) {
        lv_label_set_text(icon_label, "📁");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x42A5F5), 0);
    } else if (strstr(entry->name, ".epp")) {
        lv_label_set_text(icon_label, "📦");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x66BB6A), 0);
    } else if (strstr(entry->name, ".json")) {
        lv_label_set_text(icon_label, "📄");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFA726), 0);
    } else if (strstr(entry->name, ".txt")) {
        lv_label_set_text(icon_label, "📝");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);
    } else if (strstr(entry->name, ".mp3") || strstr(entry->name, ".wav")) {
        lv_label_set_text(icon_label, "🎵");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0xAB47BC), 0);
    } else if (strstr(entry->name, ".jpg") || strstr(entry->name, ".png") || strstr(entry->name, ".bmp")) {
        lv_label_set_text(icon_label, "🖼️");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x4DD0E1), 0);
    } else {
        lv_label_set_text(icon_label, "📃");
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x9E9E9E), 0);
    }
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
    lv_obj_center(icon_label);

    lv_obj_t *info_container = lv_obj_create(card);
    lv_obj_set_size(info_container, 280, 45);
    lv_obj_set_style_bg_color(info_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(info_container, 0, 0);
    lv_obj_set_style_border_width(info_container, 0, 0);
    lv_obj_align(info_container, LV_ALIGN_LEFT_MID, 70, 0);

    lv_obj_t *name_label = lv_label_create(info_container);
    lv_label_set_text(name_label, entry->name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *detail_label = lv_label_create(info_container);
    if (entry->is_dir) {
        lv_label_set_text(detail_label, "Folder");
    } else {
        char info_str[64];
        if (entry->size < 1024) {
            snprintf(info_str, sizeof(info_str), "%zu B", entry->size);
        } else if (entry->size < 1024 * 1024) {
            snprintf(info_str, sizeof(info_str), "%.1f KB", entry->size / 1024.0);
        } else {
            snprintf(info_str, sizeof(info_str), "%.1f MB", entry->size / (1024.0 * 1024.0));
        }
        lv_label_set_text(detail_label, info_str);
    }
    lv_obj_set_style_text_font(detail_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(0x888888), 0);
    lv_obj_align(detail_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *arrow_label = lv_label_create(card);
    lv_label_set_text(arrow_label, "›");
    lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(arrow_label, lv_color_hex(0x555555), 0);
    lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, -15, 0);

    char *full_path = (char *)heap_caps_malloc(strlen(entry->path) + 1, MALLOC_CAP_SPIRAM);
    strcpy(full_path, entry->path);
    lv_obj_set_user_data(card, full_path);

    if (entry->is_dir) {
        lv_obj_add_event_cb(card, open_dir_cb, LV_EVENT_CLICKED, NULL);
    } else if (strstr(entry->name, ".epp")) {
        lv_obj_add_event_cb(card, install_app_cb, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_add_event_cb(card, file_click_cb, LV_EVENT_CLICKED, NULL);
    }
}

static int compare_files(const void *a, const void *b)
{
    file_entry_t *fa = (file_entry_t *)a;
    file_entry_t *fb = (file_entry_t *)b;

    if (fa->is_dir != fb->is_dir) {
        return fa->is_dir ? -1 : 1;
    }

    switch (current_sort_mode) {
        case SORT_BY_NAME:
            return strcmp(fa->name, fb->name);
        case SORT_BY_SIZE:
            return (fa->size > fb->size) - (fa->size < fb->size);
        case SORT_BY_DATE:
            return (fa->mtime > fb->mtime) - (fa->mtime < fb->mtime);
        default:
            return 0;
    }
}

static void scan_directory(void)
{
    if (!permission_check("FileBrowser", PERMISSION_STORAGE)) {
        ESP_LOGW(TAG, "Storage permission denied for file browser");
        if (path_label) {
            lv_label_set_text(path_label, "Storage permission denied");
        }
        permission_request("FileBrowser", PERMISSION_STORAGE);
        return;
    }

    DIR *dir = opendir(current_path);
    if (!dir) return;

    clear_file_list();

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (file_entry_count >= MAX_FILE_ENTRIES) break;

        file_entry_t fe;
        strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
        fe.name[sizeof(fe.name) - 1] = '\0';
        memset(fe.path, 0, sizeof(fe.path));
        strncpy(fe.path, current_path, sizeof(fe.path) - 1);
        size_t len = strlen(fe.path);
        if (len + 1 < sizeof(fe.path)) {
            fe.path[len] = '/';
            len++;
        }
        strncpy(fe.path + len, entry->d_name, sizeof(fe.path) - len - 1);

        if (entry->d_type == DT_DIR) {
            fe.is_dir = 1;
            fe.size = 0;
            fe.mtime = 0;
        } else {
            fe.is_dir = 0;
            struct stat st;
            if (stat(fe.path, &st) == 0) {
                fe.size = st.st_size;
                fe.mtime = st.st_mtime;
            } else {
                fe.size = 0;
                fe.mtime = 0;
            }
        }

        file_entries[file_entry_count++] = fe;
    }

    closedir(dir);

    qsort(file_entries, file_entry_count, sizeof(file_entry_t), compare_files);

    for (int i = 0; i < file_entry_count; i++) {
        add_file_item(&file_entries[i]);
    }

    if (file_entry_count == 0) {
        lv_obj_t *empty_label = lv_label_create(list);
        lv_label_set_text(empty_label, "Empty directory");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
    }
}

static void scan_directory_recursive(const char *dir_path, int depth)
{
    if (depth >= MAX_RECURSION_DEPTH) return;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        if (file_entry_count >= MAX_FILE_ENTRIES) break;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (strstr(entry->d_name, search_query)) {
            file_entry_t fe;
            strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
            fe.name[sizeof(fe.name) - 1] = '\0';
            strncpy(fe.path, full_path, sizeof(fe.path) - 1);
            fe.path[sizeof(fe.path) - 1] = '\0';

            if (entry->d_type == DT_DIR) {
                fe.is_dir = 1;
                fe.size = 0;
                fe.mtime = 0;
            } else {
                fe.is_dir = 0;
                struct stat st;
                if (stat(fe.path, &st) == 0) {
                    fe.size = st.st_size;
                    fe.mtime = st.st_mtime;
                } else {
                    fe.size = 0;
                    fe.mtime = 0;
                }
            }

            file_entries[file_entry_count++] = fe;
        }

        if (entry->d_type == DT_DIR) {
            scan_directory_recursive(full_path, depth + 1);
        }
    }

    closedir(dir);
}

static void search_files(void)
{
    const char *query = lv_textarea_get_text(search_input);
    if (!query || strlen(query) == 0) {
        scan_directory();
        return;
    }

    snprintf(search_query, sizeof(search_query), "%s", query);
    is_deep_search = lv_obj_has_state(deep_search_cb, LV_STATE_CHECKED);

    clear_file_list();

    if (is_deep_search) {
        scan_directory_recursive(current_path, 0);
    } else {
        DIR *dir = opendir(current_path);
        if (!dir) return;

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

            if (strstr(entry->d_name, search_query) == NULL) continue;

            if (file_entry_count >= MAX_FILE_ENTRIES) break;

            file_entry_t fe;
            strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
            fe.name[sizeof(fe.name) - 1] = '\0';
            memset(fe.path, 0, sizeof(fe.path));
            strncpy(fe.path, current_path, sizeof(fe.path) - 1);
            size_t len2 = strlen(fe.path);
            if (len2 + 1 < sizeof(fe.path)) {
                fe.path[len2] = '/';
                len2++;
            }
            strncpy(fe.path + len2, entry->d_name, sizeof(fe.path) - len2 - 1);

            if (entry->d_type == DT_DIR) {
                fe.is_dir = 1;
                fe.size = 0;
                fe.mtime = 0;
            } else {
                fe.is_dir = 0;
                struct stat st;
                if (stat(fe.path, &st) == 0) {
                    fe.size = st.st_size;
                    fe.mtime = st.st_mtime;
                } else {
                    fe.size = 0;
                    fe.mtime = 0;
                }
            }

            file_entries[file_entry_count++] = fe;
        }

        closedir(dir);
    }

    qsort(file_entries, file_entry_count, sizeof(file_entry_t), compare_files);

    for (int i = 0; i < file_entry_count; i++) {
        add_file_item(&file_entries[i]);
    }

    if (file_entry_count == 0) {
        lv_obj_t *empty_label = lv_label_create(list);
        lv_label_set_text(empty_label, "No files found");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
    }
}

static void search_changed_cb(lv_event_t *e)
{
    (void)e;
    search_files();
}

static void deep_search_changed_cb(lv_event_t *e)
{
    (void)e;
    search_files();
}

static void sort_button_cb(lv_event_t *e)
{
    (void)e;

    current_sort_mode = (current_sort_mode + 1) % 3;

    const char *sort_names[] = {"Name", "Size", "Date"};
    lv_label_set_text(lv_obj_get_child(sort_btn, 0), sort_names[current_sort_mode]);

    search_files();
}

static void open_dir_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *new_path = (char *)lv_obj_get_user_data(btn);

    if (path_depth < 99) {
        size_t len = strlen(current_path) + 1;
        paths[path_depth++] = heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
        if (paths[path_depth - 1]) {
            strcpy(paths[path_depth - 1], current_path);
        }
    }
    strcpy(current_path, new_path);
    lv_label_set_text(path_label, current_path);

    scan_directory();
}

static void install_app_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *epp_path = (char *)lv_obj_get_user_data(btn);

    install_result_t ret = app_installer_install(epp_path);

    char msg[128];
    if (ret == INSTALL_OK) {
        snprintf(msg, sizeof(msg), "Installed: %s", strrchr(epp_path, '/') + 1);
        ui_feedback_show_toast("Install Success", msg, UI_FEEDBACK_TYPE_SUCCESS, 2000);
    } else if (ret == INSTALL_ERR_ALREADY_EXISTS) {
        snprintf(msg, sizeof(msg), "Already installed!");
        ui_feedback_show_toast("Warning", msg, UI_FEEDBACK_TYPE_WARNING, 2000);
    } else {
        snprintf(msg, sizeof(msg), "Install failed!");
        ui_feedback_show_toast("Install Failed", msg, UI_FEEDBACK_TYPE_ERROR, 2000);
    }
}

static void delete_file_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *file_path = (char *)lv_obj_get_user_data(btn);

    if (remove(file_path) == 0) {
        ui_feedback_show_toast("Success", "File deleted", UI_FEEDBACK_TYPE_SUCCESS, 2000);
    } else {
        ui_feedback_show_toast("Error", "Delete failed", UI_FEEDBACK_TYPE_ERROR, 2000);
    }

    heap_caps_free(file_path);
    lv_obj_del(lv_obj_get_parent(btn));
    scan_directory();
}

static void rename_file_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *file_path = (char *)lv_obj_get_user_data(btn);

    const char *new_name = lv_textarea_get_text(current_dialog_rename_input);

    if (!new_name || strlen(new_name) == 0) {
        ui_feedback_show_toast("Error", "Invalid name", UI_FEEDBACK_TYPE_ERROR, 2000);
        return;
    }

    char dir_path[512];
    char *last_slash = strrchr(file_path, '/');
    if (last_slash) {
        strncpy(dir_path, file_path, last_slash - file_path + 1);
        dir_path[last_slash - file_path + 1] = '\0';
    } else {
        strcpy(dir_path, "/");
    }

    char new_path[512];
    snprintf(new_path, sizeof(new_path), "%s%s", dir_path, new_name);

    if (rename(file_path, new_path) == 0) {
        ui_feedback_show_toast("Success", "File renamed", UI_FEEDBACK_TYPE_SUCCESS, 2000);
    } else {
        ui_feedback_show_toast("Error", "Rename failed", UI_FEEDBACK_TYPE_ERROR, 2000);
    }

    heap_caps_free(file_path);
    lv_obj_del(current_dialog);
    scan_directory();
}

static void close_dialog_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_del(lv_obj_get_parent(btn));
}

static void dialog_delete_cb(lv_event_t *e)
{
    lv_obj_t *dialog = lv_event_get_target(e);

    uint32_t child_count = lv_obj_get_child_count(dialog);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(dialog, i);
        if (child) {
            void *user_data = lv_obj_get_user_data(child);
            if (user_data) {
                heap_caps_free(user_data);
                lv_obj_set_user_data(child, NULL);
            }
        }
    }
}

static void file_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    char *file_path = (char *)lv_obj_get_user_data(btn);

    struct stat st;
    if (stat(file_path, &st) != 0) {
        ui_feedback_show_toast("Error", "Cannot read file", UI_FEEDBACK_TYPE_ERROR, 2000);
        return;
    }

    char size_str[32];
    if (st.st_size < 1024) {
        snprintf(size_str, sizeof(size_str), "%lld bytes", (long long)st.st_size);
    } else if (st.st_size < 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size / (1024.0 * 1024.0));
    }

    char time_str[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);

    current_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(current_dialog, 340, 300);
    lv_obj_set_style_bg_color(current_dialog, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(current_dialog, 0, 0);
    lv_obj_set_style_radius(current_dialog, 16, 0);
    lv_obj_set_style_shadow_width(current_dialog, 10, 0);
    lv_obj_set_style_shadow_color(current_dialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(current_dialog, 80, 0);
    lv_obj_center(current_dialog);
    lv_obj_add_event_cb(current_dialog, dialog_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *title_label = lv_label_create(current_dialog);
    lv_label_set_text(title_label, "File Details");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 15);

    const char *filename = strrchr(file_path, '/');
    if (!filename) filename = file_path;
    else filename++;

    lv_obj_t *name_label = lv_label_create(current_dialog);
    char name_str[128];
    snprintf(name_str, sizeof(name_str), "Name: %s", filename);
    lv_label_set_text(name_label, name_str);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 20, 50);

    lv_obj_t *size_label = lv_label_create(current_dialog);
    char size_display[128];
    snprintf(size_display, sizeof(size_display), "Size: %s", size_str);
    lv_label_set_text(size_label, size_display);
    lv_obj_set_style_text_font(size_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(size_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(size_label, LV_ALIGN_TOP_LEFT, 20, 75);

    lv_obj_t *time_label = lv_label_create(current_dialog);
    char time_display[128];
    snprintf(time_display, sizeof(time_display), "Modified: %s", time_str);
    lv_label_set_text(time_label, time_display);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 20, 100);

    lv_obj_t *rename_label = lv_label_create(current_dialog);
    lv_label_set_text(rename_label, "New name:");
    lv_obj_set_style_text_font(rename_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rename_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(rename_label, LV_ALIGN_TOP_LEFT, 20, 135);

    current_dialog_rename_input = lv_textarea_create(current_dialog);
    lv_obj_set_size(current_dialog_rename_input, 240, 40);
    lv_textarea_set_text(current_dialog_rename_input, filename);
    lv_obj_set_style_bg_color(current_dialog_rename_input, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(current_dialog_rename_input, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_border_width(current_dialog_rename_input, 1, 0);
    lv_obj_set_style_text_color(current_dialog_rename_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(current_dialog_rename_input, 8, 0);
    lv_obj_align(current_dialog_rename_input, LV_ALIGN_TOP_LEFT, 20, 160);

    lv_obj_t *delete_btn = lv_btn_create(current_dialog);
    lv_obj_set_size(delete_btn, 130, 40);
    lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xC62828), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(delete_btn, 0, 0);
    lv_obj_set_style_radius(delete_btn, 8, 0);
    lv_obj_align(delete_btn, LV_ALIGN_BOTTOM_LEFT, 20, -15);
    size_t file_path_len = strlen(file_path) + 1;
    char *delete_path_copy = heap_caps_malloc(file_path_len, MALLOC_CAP_SPIRAM);
    if (delete_path_copy) {
        strcpy(delete_path_copy, file_path);
        lv_obj_set_user_data(delete_btn, delete_path_copy);
    }
    lv_obj_add_event_cb(delete_btn, delete_file_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *delete_label = lv_label_create(delete_btn);
    lv_label_set_text(delete_label, "Delete");
    lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(delete_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(delete_label);

    lv_obj_t *rename_btn = lv_btn_create(current_dialog);
    lv_obj_set_size(rename_btn, 130, 40);
    lv_obj_set_style_bg_color(rename_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_bg_color(rename_btn, lv_color_hex(0x388E3C), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(rename_btn, 0, 0);
    lv_obj_set_style_radius(rename_btn, 8, 0);
    lv_obj_align(rename_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -15);
    char *rename_path_copy = heap_caps_malloc(file_path_len, MALLOC_CAP_SPIRAM);
    if (rename_path_copy) {
        strcpy(rename_path_copy, file_path);
        lv_obj_set_user_data(rename_btn, rename_path_copy);
    }
    lv_obj_add_event_cb(rename_btn, rename_file_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *rename_btn_label = lv_label_create(rename_btn);
    lv_label_set_text(rename_btn_label, "Rename");
    lv_obj_set_style_text_font(rename_btn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(rename_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(rename_btn_label);

    lv_obj_t *close_btn = lv_btn_create(current_dialog);
    lv_obj_set_size(close_btn, 40, 40);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_radius(close_btn, 20, 0);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "✕");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(close_label);

    lv_obj_add_event_cb(close_btn, close_dialog_cb, LV_EVENT_CLICKED, NULL);
}

static void spiffs_btn_cb(lv_event_t *e)
{
    (void)e;
    free_paths();
    strcpy(current_path, "/spiffs");
    lv_label_set_text(path_label, current_path);
    scan_directory();
}

static void sdcard_btn_cb(lv_event_t *e)
{
    (void)e;
    free_paths();
    strcpy(current_path, "/sdcard");
    lv_label_set_text(path_label, current_path);
    scan_directory();
}

lv_obj_t *file_browser_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "File Browser");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *path_container = lv_obj_create(scr);
    lv_obj_set_size(path_container, 440, 35);
    lv_obj_set_style_bg_color(path_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(path_container, 0, 0);
    lv_obj_set_style_radius(path_container, 8, 0);
    lv_obj_align(path_container, LV_ALIGN_TOP_MID, 0, 85);

    path_label = lv_label_create(path_container);
    lv_label_set_text(path_label, current_path);
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(path_label, lv_color_hex(0x87CEEB), 0);
    lv_obj_align(path_label, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *search_input = lv_textarea_create(scr);
    lv_obj_set_size(search_input, 260, 40);
    lv_textarea_set_placeholder_text(search_input, "Search...");
    lv_obj_set_style_bg_color(search_input, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(search_input, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_border_width(search_input, 0, 0);
    lv_obj_set_style_text_color(search_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(search_input, 8, 0);
    lv_obj_align(search_input, LV_ALIGN_TOP_LEFT, 20, 130);
    lv_obj_add_event_cb(search_input, search_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    deep_search_cb = lv_checkbox_create(scr);
    lv_checkbox_set_text(deep_search_cb, "Deep");
    lv_obj_set_style_text_font(deep_search_cb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(deep_search_cb, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_bg_color(deep_search_cb, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(deep_search_cb, 0, 0);
    lv_obj_align(deep_search_cb, LV_ALIGN_TOP_LEFT, 290, 138);
    lv_obj_add_event_cb(deep_search_cb, deep_search_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    sort_btn = lv_btn_create(scr);
    lv_obj_set_size(sort_btn, 70, 40);
    lv_obj_set_style_bg_color(sort_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(sort_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sort_btn, 0, 0);
    lv_obj_set_style_radius(sort_btn, 8, 0);
    lv_obj_align(sort_btn, LV_ALIGN_TOP_RIGHT, -20, 130);
    lv_obj_add_event_cb(sort_btn, sort_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sort_label = lv_label_create(sort_btn);
    lv_label_set_text(sort_label, "Name");
    lv_obj_set_style_text_font(sort_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sort_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(sort_label);

    lv_obj_t *up_btn = lv_btn_create(scr);
    lv_obj_set_size(up_btn, 35, 35);
    lv_obj_set_style_bg_color(up_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(up_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(up_btn, 0, 0);
    lv_obj_set_style_radius(up_btn, 8, 0);
    lv_obj_align(up_btn, LV_ALIGN_TOP_RIGHT, -100, 87);
    lv_obj_add_event_cb(up_btn, up_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *up_label = lv_label_create(up_btn);
    lv_label_set_text(up_label, "↑");
    lv_obj_set_style_text_font(up_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(up_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(up_label);

    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 450);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(list_container, 0, 0);
    lv_obj_set_style_radius(list_container, 12, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 180);

    list = lv_list_create(list_container);
    lv_obj_set_size(list, 420, 430);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *spiffs_btn = lv_btn_create(scr);
    lv_obj_set_size(spiffs_btn, 140, 45);
    lv_obj_set_style_bg_color(spiffs_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(spiffs_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(spiffs_btn, 0, 0);
    lv_obj_set_style_radius(spiffs_btn, 8, 0);
    lv_obj_align(spiffs_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    lv_obj_t *spiffs_label = lv_label_create(spiffs_btn);
    lv_label_set_text(spiffs_label, "📁 SPIFFS");
    lv_obj_set_style_text_font(spiffs_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(spiffs_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(spiffs_label);

    lv_obj_add_event_cb(spiffs_btn, spiffs_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sdcard_btn = lv_btn_create(scr);
    lv_obj_set_size(sdcard_btn, 140, 45);
    lv_obj_set_style_bg_color(sdcard_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(sdcard_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(sdcard_btn, 0, 0);
    lv_obj_set_style_radius(sdcard_btn, 8, 0);
    lv_obj_align(sdcard_btn, LV_ALIGN_BOTTOM_LEFT, 175, -20);

    lv_obj_t *sdcard_label = lv_label_create(sdcard_btn);
    lv_label_set_text(sdcard_label, "💾 SD Card");
    lv_obj_set_style_text_font(sdcard_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sdcard_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(sdcard_label);

    lv_obj_add_event_cb(sdcard_btn, sdcard_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 90, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "← Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    input_tool = input_tool_create(scr);
    input_tool_attach_textarea(input_tool, search_input);

    scan_directory();

    ui_animation_slide(scr, LV_DIR_RIGHT, 300);

    ESP_LOGI(TAG, "File browser app created with card design");
    return scr;
}