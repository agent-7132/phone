#include "reader_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "bt_manager.h"
#include "theme_manager.h"
#include "permission_manager.h"
#include "ui_animation.h"
#include "ui_feedback.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <dirent.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

static const char *TAG = "READER_APP";

#define MAX_BOOKS 50
#define MAX_PATH_LEN 260
#define MAX_TEXT_LEN 256 * 1024
#define MAX_LINES_PER_PAGE 30
#define MAX_BOOKMARKS 10
#define MAX_CHAPTERS 100
#define MAX_HISTORY 5
#define MAX_SEARCH_RESULTS 30

typedef enum {
    READER_MODE_LIST,
    READER_MODE_READING,
    READER_MODE_BOOKMARKS,
    READER_MODE_SEARCH
} reader_mode_t;

typedef enum {
    FORMAT_TXT,
    FORMAT_EPUB,
    FORMAT_MOBI,
    FORMAT_PDF,
    FORMAT_UNKNOWN
} book_format_t;

typedef struct {
    char title[128];
    char path[MAX_PATH_LEN];
    book_format_t format;
    size_t file_size;
} book_info_t;

typedef struct {
    char book_path[MAX_PATH_LEN];
    int page;
    int total_pages;
    char title[128];
    time_t timestamp;
} bookmark_t;

typedef struct {
    char title[128];
    size_t start_offset;
    size_t end_offset;
    int page_count;
} chapter_t;

typedef struct {
    char book_path[MAX_PATH_LEN];
    char title[128];
    int last_page;
    int total_pages;
    int reading_time;
    time_t last_read;
} reading_history_t;

typedef struct {
    int page;
    int line;
    char snippet[128];
} search_result_t;

static lv_obj_t *scr = NULL;
static lv_obj_t *book_list = NULL;
static lv_obj_t *text_area = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *progress_bar = NULL;
static lv_obj_t *page_label = NULL;
static lv_obj_t *speed_label = NULL;
static lv_obj_t *search_input = NULL;
static lv_obj_t *search_results = NULL;

static book_info_t books[MAX_BOOKS];
static int book_count = 0;
static int current_book = -1;

static char *book_content = NULL;
static size_t content_size = 0;
static int total_pages = 0;
static int current_page = 0;
static int font_size = 24;
static int scroll_speed = 0;
static bool is_auto_scrolling = false;
static int sleep_timer = 0;

static reader_mode_t current_mode = READER_MODE_LIST;

static esp_codec_dev_handle_t speaker = NULL;
static int volume = 50;

static bookmark_t bookmarks[MAX_BOOKMARKS];
static int bookmark_count = 0;

static chapter_t chapters[MAX_CHAPTERS];
static int chapter_count = 0;
static int current_chapter = 0;

static reading_history_t history[MAX_HISTORY];
static int history_count = 0;

static search_result_t search_results_array[MAX_SEARCH_RESULTS];
static int search_result_count = 0;
static char search_query[256] = {0};

static book_format_t get_format(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (!ext) return FORMAT_UNKNOWN;
    
    if (strcmp(ext, ".txt") == 0 || strcmp(ext, ".TXT") == 0) return FORMAT_TXT;
    if (strcmp(ext, ".epub") == 0 || strcmp(ext, ".EPUB") == 0) return FORMAT_EPUB;
    if (strcmp(ext, ".mobi") == 0 || strcmp(ext, ".MOBI") == 0) return FORMAT_MOBI;
    if (strcmp(ext, ".pdf") == 0 || strcmp(ext, ".PDF") == 0) return FORMAT_PDF;
    
    return FORMAT_UNKNOWN;
}

static const char *format_to_str(book_format_t format)
{
    switch (format) {
        case FORMAT_TXT: return "TXT";
        case FORMAT_EPUB: return "EPUB";
        case FORMAT_MOBI: return "MOBI";
        case FORMAT_PDF: return "PDF";
        default: return "Unknown";
    }
}

static bool is_book_file(const char *filename)
{
    return get_format(filename) != FORMAT_UNKNOWN;
}

static void scan_books(void)
{
    book_count = 0;
    memset(books, 0, sizeof(books));
    
    if (!permission_check("Reader", PERMISSION_STORAGE)) {
        ESP_LOGW(TAG, "Storage permission denied for book scan");
        if (status_label) {
            lv_label_set_text(status_label, "Storage permission denied");
        }
        permission_request("Reader", PERMISSION_STORAGE);
        return;
    }

    const char *paths[] = {"/sdcard/books", "/sdcard", "/spiffs/books", "/spiffs"};
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]) && book_count < MAX_BOOKS; i++) {
        DIR *dir = opendir(paths[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && book_count < MAX_BOOKS) {
            if (entry->d_name[0] == '.') continue;
            if (!is_book_file(entry->d_name)) continue;

            snprintf(books[book_count].path, MAX_PATH_LEN, "%s/%s", paths[i], entry->d_name);
            
            const char *name = strrchr(entry->d_name, '.');
            if (name) {
                strncpy(books[book_count].title, entry->d_name, name - entry->d_name);
                books[book_count].title[name - entry->d_name] = '\0';
            } else {
                strcpy(books[book_count].title, entry->d_name);
            }
            
            books[book_count].format = get_format(entry->d_name);
            
            FILE *f = fopen(books[book_count].path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                books[book_count].file_size = ftell(f);
                fclose(f);
            } else {
                books[book_count].file_size = 0;
            }
            
            book_count++;
        }
        closedir(dir);
    }
}

static int is_chapter_heading(const char *line, size_t line_len)
{
    if (line_len < 4 || line_len > 64) return 0;
    
    if (line[0] == '\n' || line[0] == '\r') line++;
    
    if (strncmp(line, "第", 1) == 0) {
        int i = 1;
        while (i < (int)line_len && (line[i] >= '0' && line[i] <= '9')) i++;
        if (i > 1 && i < (int)line_len) {
            if (strncmp(line + i, "章", 1) == 0) return 1;
            if (strncmp(line + i, "回", 1) == 0) return 1;
            if (strncmp(line + i, "节", 1) == 0) return 1;
            if (strncmp(line + i, "卷", 1) == 0) return 1;
            if (strncmp(line + i, "篇", 1) == 0) return 1;
            if (strncmp(line + i, "部", 1) == 0) return 1;
            if (strncmp(line + i, "幕", 1) == 0) return 1;
        }
    }
    
    if (strncmp(line, "卷", 1) == 0) {
        int i = 1;
        while (i < (int)line_len && (line[i] >= '0' && line[i] <= '9')) i++;
        if (i > 1) return 1;
    }
    
    return 0;
}

static void detect_chapters(void)
{
    chapter_count = 0;
    memset(chapters, 0, sizeof(chapters));
    
    if (!book_content || content_size == 0) {
        chapters[0].start_offset = 0;
        chapters[0].end_offset = content_size;
        strcpy(chapters[0].title, "全文");
        chapter_count = 1;
        return;
    }
    
    size_t start = 0;
    size_t i = 0;
    
    while (i < content_size && chapter_count < MAX_CHAPTERS) {
        if (book_content[i] == '\n') {
            size_t line_start = start;
            size_t line_len = i - line_start;
            
            if (line_len > 0 && is_chapter_heading(book_content + line_start, line_len)) {
                if (chapter_count > 0) {
                    chapters[chapter_count - 1].end_offset = line_start;
                }
                
                size_t title_end = i;
                while (title_end > line_start && 
                       (book_content[title_end - 1] == ' ' || 
                        book_content[title_end - 1] == '\t' ||
                        book_content[title_end - 1] == '\r')) {
                    title_end--;
                }
                
                size_t title_len = title_end - line_start;
                if (title_len > sizeof(chapters[chapter_count].title) - 1) {
                    title_len = sizeof(chapters[chapter_count].title) - 1;
                }
                
                strncpy(chapters[chapter_count].title, book_content + line_start, title_len);
                chapters[chapter_count].title[title_len] = '\0';
                chapters[chapter_count].start_offset = line_start;
                
                chapter_count++;
                ESP_LOGI(TAG, "Found chapter: %s at offset %zu", 
                         chapters[chapter_count - 1].title, 
                         chapters[chapter_count - 1].start_offset);
            }
            
            start = i + 1;
        }
        i++;
    }
    
    if (chapter_count > 0) {
        chapters[chapter_count - 1].end_offset = content_size;
    }
    
    if (chapter_count == 0) {
        chapters[0].start_offset = 0;
        chapters[0].end_offset = content_size;
        strcpy(chapters[0].title, "全文");
        chapter_count = 1;
    }
    
    ESP_LOGI(TAG, "Detected %d chapters", chapter_count);
}

static size_t read_file_content(const char *path)
{
    if (book_content) {
        heap_caps_free(book_content);
        book_content = NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > MAX_TEXT_LEN) {
        size = MAX_TEXT_LEN;
    }

    book_content = (char *)heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
    if (!book_content) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate memory in PSRAM");
        return 0;
    }

    size_t read = fread(book_content, 1, size, f);
    book_content[read] = '\0';
    fclose(f);

    for (size_t i = 0; i < read; i++) {
        if (book_content[i] == '\r') {
            book_content[i] = '\n';
        }
    }

    return read;
}

static int calculate_chapter_pages(int chapter_idx)
{
    if (!book_content || content_size == 0 || chapter_idx < 0 || chapter_idx >= chapter_count) return 0;

    int lines = 0;
    int chars = 0;
    int max_chars_per_line = 480 / (font_size / 2);
    size_t start = chapters[chapter_idx].start_offset;
    size_t end = chapters[chapter_idx].end_offset;

    for (size_t i = start; i < end; i++) {
        if (book_content[i] == '\n') {
            lines++;
            chars = 0;
        } else {
            chars++;
            if (chars >= max_chars_per_line) {
                lines++;
                chars = 0;
            }
        }
    }

    if (chars > 0) lines++;
    return (lines + MAX_LINES_PER_PAGE - 1) / MAX_LINES_PER_PAGE;
}

static int calculate_pages(void)
{
    if (!book_content || content_size == 0) return 0;

    int total = 0;
    for (int i = 0; i < chapter_count; i++) {
        chapters[i].page_count = calculate_chapter_pages(i);
        total += chapters[i].page_count;
    }
    return total;
}

static void render_page(int page)
{
    if (!text_area || !book_content) return;

    int lines = 0;
    int chars = 0;
    int max_chars_per_line = 480 / (font_size / 2);
    
    int accumulated_pages = 0;
    int target_chapter = 0;
    int chapter_page = 0;
    
    for (int i = 0; i < chapter_count; i++) {
        if (accumulated_pages + chapters[i].page_count > page) {
            target_chapter = i;
            chapter_page = page - accumulated_pages;
            break;
        }
        accumulated_pages += chapters[i].page_count;
    }
    
    if (target_chapter >= chapter_count) target_chapter = chapter_count - 1;
    
    current_chapter = target_chapter;
    
    size_t start_pos = chapters[target_chapter].start_offset;
    size_t end_pos = chapters[target_chapter].end_offset;
    size_t content_start = start_pos;
    size_t content_end = end_pos;
    
    int total_lines = 0;
    for (size_t i = start_pos; i < end_pos; i++) {
        if (book_content[i] == '\n') {
            total_lines++;
        } else {
            chars++;
            if (chars >= max_chars_per_line) {
                total_lines++;
                chars = 0;
            }
        }
    }
    if (chars > 0) total_lines++;
    
    lines = 0;
    chars = 0;
    bool started = false;
    content_start = start_pos;
    content_end = end_pos;
    
    for (size_t i = start_pos; i < end_pos; i++) {
        if (book_content[i] == '\n') {
            if (started && lines >= MAX_LINES_PER_PAGE) {
                content_end = i;
                break;
            }
            lines++;
            chars = 0;
            if (!started && lines > chapter_page * MAX_LINES_PER_PAGE) {
                started = true;
                content_start = i + 1;
            }
        } else {
            chars++;
            if (chars >= max_chars_per_line) {
                if (started && lines >= MAX_LINES_PER_PAGE) {
                    content_end = i;
                    break;
                }
                lines++;
                chars = 0;
                if (!started && lines > chapter_page * MAX_LINES_PER_PAGE) {
                    started = true;
                    content_start = i + 1;
                }
            }
        }
    }

    if (content_end == start_pos) content_end = end_pos;
    
    char page_text[4096] = {0};
    int len = (int)(content_end - content_start);
    if (len > (int)sizeof(page_text) - 1) len = (int)sizeof(page_text) - 1;
    snprintf(page_text, sizeof(page_text), "%.*s", len, book_content + content_start);
    
    lv_label_set_text(text_area, page_text);
}

static esp_err_t save_reading_history(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    
    char key[32];
    for (int i = 0; i < history_count; i++) {
        snprintf(key, sizeof(key), "history_%d", i);
        char value[1024];
        size_t path_len = strlen(history[i].book_path);
        size_t title_len = strlen(history[i].title);
        size_t max_path_len = (sizeof(value) - 40) / 2;
        size_t max_title_len = sizeof(value) - 40 - max_path_len;
        snprintf(value, sizeof(value), "%.*s|%.*s|%d|%d|%d|%lld", 
                 (int)(path_len > max_path_len ? max_path_len : path_len), history[i].book_path, 
                 (int)(title_len > max_title_len ? max_title_len : title_len), history[i].title, 
                 history[i].last_page, history[i].total_pages,
                 history[i].reading_time, (long long)history[i].last_read);
        err = nvs_set_str(handle, key, value);
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }
    
    err = nvs_set_i32(handle, "history_count", history_count);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    nvs_commit(handle);
    nvs_close(handle);
    return ESP_OK;
}

static esp_err_t load_reading_history(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    
    int32_t history_count_temp = 0;
    err = nvs_get_i32(handle, "history_count", &history_count_temp);
    history_count = (int)history_count_temp;
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    }
    
    if (history_count > MAX_HISTORY) history_count = MAX_HISTORY;
    
    char key[32];
    for (int i = 0; i < history_count; i++) {
        snprintf(key, sizeof(key), "history_%d", i);
        char value[512];
        size_t len = sizeof(value);
        err = nvs_get_str(handle, key, value, &len);
        if (err != ESP_OK) continue;
        
        sscanf(value, "%511[^|]|%127[^|]|%d|%d|%d|%lld", 
               history[i].book_path, history[i].title, 
               &history[i].last_page, &history[i].total_pages,
               &history[i].reading_time, (long long *)&history[i].last_read);
    }
    
    nvs_close(handle);
    return ESP_OK;
}

static void update_reading_history(void)
{
    if (current_book < 0 || current_book >= book_count) return;
    
    for (int i = 0; i < history_count; i++) {
        if (strcmp(history[i].book_path, books[current_book].path) == 0) {
            history[i].last_page = current_page;
            history[i].total_pages = total_pages;
            history[i].last_read = time(NULL);
            history[i].reading_time += 1;
            save_reading_history();
            return;
        }
    }
    
    if (history_count < MAX_HISTORY) {
        strcpy(history[history_count].book_path, books[current_book].path);
        strcpy(history[history_count].title, books[current_book].title);
        history[history_count].last_page = current_page;
        history[history_count].total_pages = total_pages;
        history[history_count].reading_time = 1;
        history[history_count].last_read = time(NULL);
        history_count++;
        save_reading_history();
    }
}

static void save_bookmarks(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader", NVS_READWRITE, &handle);
    if (err != ESP_OK) return;
    
    char key[32];
    for (int i = 0; i < bookmark_count; i++) {
        snprintf(key, sizeof(key), "bookmark_%d", i);
        char value[1024];
        size_t path_len = strlen(bookmarks[i].book_path);
        size_t title_len = strlen(bookmarks[i].title);
        size_t max_path_len = (sizeof(value) - 30) / 2;
        size_t max_title_len = sizeof(value) - 30 - max_path_len;
        snprintf(value, sizeof(value), "%.*s|%d|%d|%.*s", 
                 (int)(path_len > max_path_len ? max_path_len : path_len), bookmarks[i].book_path, 
                 bookmarks[i].page, bookmarks[i].total_pages,
                 (int)(title_len > max_title_len ? max_title_len : title_len), bookmarks[i].title);
        nvs_set_str(handle, key, value);
    }
    
    nvs_set_i32(handle, "bookmark_count", bookmark_count);
    nvs_commit(handle);
    nvs_close(handle);
}

static void load_bookmarks(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("reader", NVS_READONLY, &handle);
    if (err != ESP_OK) return;
    
    int32_t bookmark_count_temp = 0;
    err = nvs_get_i32(handle, "bookmark_count", &bookmark_count_temp);
    bookmark_count = (int)bookmark_count_temp;
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return;
    }
    
    if (bookmark_count > MAX_BOOKMARKS) bookmark_count = MAX_BOOKMARKS;
    
    char key[32];
    for (int i = 0; i < bookmark_count; i++) {
        snprintf(key, sizeof(key), "bookmark_%d", i);
        char value[512];
        size_t len = sizeof(value);
        err = nvs_get_str(handle, key, value, &len);
        if (err != ESP_OK) continue;
        
        sscanf(value, "%511[^|]|%d|%d|%127[^|]", 
               bookmarks[i].book_path, &bookmarks[i].page, 
               &bookmarks[i].total_pages, bookmarks[i].title);
    }
    
    nvs_close(handle);
}

static void search_text(const char *query)
{
    if (!query || !book_content || strlen(query) < 2) {
        search_result_count = 0;
        return;
    }
    
    search_result_count = 0;
    size_t query_len = strlen(query);
    
    for (size_t i = 0; i < content_size - query_len && search_result_count < MAX_SEARCH_RESULTS; i++) {
        bool match = true;
        for (size_t j = 0; j < query_len; j++) {
            char c1 = book_content[i + j];
            char c2 = query[j];
            
            if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
            if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
            
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        
        if (match) {
            search_results_array[search_result_count].page = current_page;
            
            size_t line_start = i;
            while (line_start > 0 && book_content[line_start - 1] != '\n') line_start--;
            
            size_t line_end = i + query_len;
            while (line_end < content_size && book_content[line_end] != '\n') line_end++;
            
            size_t snippet_len = line_end - line_start;
            if (snippet_len > sizeof(search_results_array[search_result_count].snippet) - 1) {
                snippet_len = sizeof(search_results_array[search_result_count].snippet) - 1;
            }
            
            strncpy(search_results_array[search_result_count].snippet, 
                    book_content + line_start, snippet_len);
            search_results_array[search_result_count].snippet[snippet_len] = '\0';
            
            search_result_count++;
        }
    }
}

static void load_book(int index);
static void create_reading_ui(void);
static void create_list_ui(void);
static void create_bookmark_ui(void);
static void create_search_ui(void);
static void back_button_cb(lv_event_t *e);
static void jump_to_search_result(lv_event_t *e);

static void load_book(int index)
{
    if (index < 0 || index >= book_count) return;

    current_book = index;
    content_size = read_file_content(books[current_book].path);
    
    if (content_size == 0) {
        lv_label_set_text(status_label, "Failed to load book");
        return;
    }

    detect_chapters();
    total_pages = calculate_pages();
    current_page = 0;
    current_chapter = 0;

    char status[256];
    snprintf(status, sizeof(status), "%.100s (%d pages, %d chapters)", 
             books[current_book].title, total_pages, chapter_count);
    lv_label_set_text(status_label, status);

    create_reading_ui();
    render_page(current_page);
    lv_slider_set_range(progress_bar, 0, total_pages - 1);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    
    current_mode = READER_MODE_READING;
    
    load_reading_history();
    for (int i = 0; i < history_count; i++) {
        if (strcmp(history[i].book_path, books[current_book].path) == 0) {
            current_page = history[i].last_page;
            render_page(current_page);
            lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
            char page_str[64];
            snprintf(page_str, sizeof(page_str), "%d/%d", current_page + 1, total_pages);
            lv_label_set_text(page_label, page_str);
            break;
        }
    }
}

static void prev_page(lv_event_t *e)
{
    (void)e;
    if (current_mode != READER_MODE_READING || current_page <= 0) return;
    
    current_page--;
    render_page(current_page);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    
    update_reading_history();
    
    char status[128];
    snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, status);
}

static void next_page(lv_event_t *e)
{
    (void)e;
    if (current_mode != READER_MODE_READING || current_page >= total_pages - 1) return;
    
    current_page++;
    render_page(current_page);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    
    update_reading_history();
    
    char status[128];
    snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, status);
}

static void page_changed(lv_event_t *e)
{
    if (current_mode != READER_MODE_READING) return;
    
    current_page = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    render_page(current_page);
    update_reading_history();
    
    char status[128];
    snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, status);
}

static void increase_font(lv_event_t *e)
{
    (void)e;
    if (font_size < 40) {
        font_size += 2;
        lv_obj_set_style_text_font(text_area, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_line_space(text_area, font_size / 2, 0);
        
        total_pages = calculate_pages();
        if (current_page >= total_pages) current_page = total_pages - 1;
        render_page(current_page);
        lv_slider_set_range(progress_bar, 0, total_pages - 1);
        lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
        
        char status[128];
        snprintf(status, sizeof(status), "Size: %d", font_size);
        lv_label_set_text(page_label, status);
    }
}

static void decrease_font(lv_event_t *e)
{
    (void)e;
    if (font_size > 16) {
        font_size -= 2;
        lv_obj_set_style_text_font(text_area, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_line_space(text_area, font_size / 2, 0);
        
        total_pages = calculate_pages();
        render_page(current_page);
        lv_slider_set_range(progress_bar, 0, total_pages - 1);
        
        char status[128];
        snprintf(status, sizeof(status), "Size: %d", font_size);
        lv_label_set_text(page_label, status);
    }
}

static void toggle_auto_scroll(lv_event_t *e)
{
    (void)e;
    if (current_mode != READER_MODE_READING) return;
    
    is_auto_scrolling = !is_auto_scrolling;
    lv_label_set_text(speed_label, is_auto_scrolling ? "Auto: On" : "Auto: Off");
}

static void speed_up(lv_event_t *e)
{
    (void)e;
    if (scroll_speed < 10) {
        scroll_speed++;
        char status[32];
        snprintf(status, sizeof(status), "Speed: %d", scroll_speed);
        lv_label_set_text(speed_label, status);
    }
}

static void speed_down(lv_event_t *e)
{
    (void)e;
    if (scroll_speed > 0) {
        scroll_speed--;
        char status[32];
        snprintf(status, sizeof(status), "Speed: %d", scroll_speed);
        lv_label_set_text(speed_label, status);
    }
}

static void add_bookmark(lv_event_t *e)
{
    (void)e;
    if (current_mode != READER_MODE_READING || bookmark_count >= MAX_BOOKMARKS) return;
    
    strcpy(bookmarks[bookmark_count].book_path, books[current_book].path);
    strcpy(bookmarks[bookmark_count].title, books[current_book].title);
    bookmarks[bookmark_count].page = current_page;
    bookmarks[bookmark_count].total_pages = total_pages;
    bookmarks[bookmark_count].timestamp = time(NULL);
    bookmark_count++;
    
    save_bookmarks();
    
    char status[128];
    snprintf(status, sizeof(status), "Bookmark: Page %d", current_page + 1);
    lv_label_set_text(page_label, status);
    
    ui_feedback_show_toast("Bookmark Added", status, UI_FEEDBACK_TYPE_SUCCESS, 2000);
}

static void remove_bookmark(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index < 0 || index >= bookmark_count) return;
    
    for (int i = index; i < bookmark_count - 1; i++) {
        bookmarks[i] = bookmarks[i + 1];
    }
    bookmark_count--;
    save_bookmarks();
    
    create_bookmark_ui();
    ui_feedback_show_toast("Bookmark Removed", "Bookmark deleted", UI_FEEDBACK_TYPE_INFO, 2000);
}

static void jump_to_bookmark(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index < 0 || index >= bookmark_count) return;
    
    for (int i = 0; i < book_count; i++) {
        if (strcmp(books[i].path, bookmarks[index].book_path) == 0) {
            load_book(i);
            current_page = bookmarks[index].page;
            render_page(current_page);
            lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
            char page_str[64];
            snprintf(page_str, sizeof(page_str), "%d/%d", current_page + 1, total_pages);
            lv_label_set_text(page_label, page_str);
            return;
        }
    }
    
    ui_feedback_show_toast("Error", "Book not found", UI_FEEDBACK_TYPE_ERROR, 2000);
}

static void toggle_theme(lv_event_t *e)
{
    (void)e;
    theme_mode_t current_mode = theme_manager_get_mode();
    theme_mode_t new_mode = (current_mode == THEME_MODE_DARK) ? THEME_MODE_LIGHT : THEME_MODE_DARK;
    theme_manager_set_mode(new_mode);
}

static void volume_up(lv_event_t *e)
{
    (void)e;
    if (volume < 100) {
        volume += 10;
        if (speaker) {
            esp_codec_dev_set_out_vol(speaker, volume);
        }
        char status[32];
        snprintf(status, sizeof(status), "Vol: %d%%", volume);
        lv_label_set_text(page_label, status);
    }
}

static void volume_down(lv_event_t *e)
{
    (void)e;
    if (volume > 0) {
        volume -= 10;
        if (speaker) {
            esp_codec_dev_set_out_vol(speaker, volume);
        }
        char status[32];
        snprintf(status, sizeof(status), "Vol: %d%%", volume);
        lv_label_set_text(page_label, status);
    }
}

static void set_sleep_timer(lv_event_t *e)
{
    (void)e;
    sleep_timer = (sleep_timer + 15) % 121;
    if (sleep_timer == 0) {
        lv_label_set_text(speed_label, "Timer: Off");
    } else {
        char status[32];
        snprintf(status, sizeof(status), "Timer: %dm", sleep_timer);
        lv_label_set_text(speed_label, status);
    }
}

static void jump_to_chapter(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    lv_obj_t *btn = lv_event_get_target(e);
    int chapter_idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (chapter_idx < 0 || chapter_idx >= chapter_count) return;
    
    int target_page = 0;
    for (int i = 0; i < chapter_idx; i++) {
        target_page += chapters[i].page_count;
    }
    
    current_page = target_page;
    current_chapter = chapter_idx;
    
    render_page(current_page);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    
    char status[128];
    snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, status);
    
    create_reading_ui();
}

static void close_chapter_list(lv_event_t *e)
{
    (void)e;
    create_reading_ui();
}

static void show_chapter_list(lv_event_t *e)
{
    (void)e;
    
    lv_obj_clean(scr);
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "📋 Chapters");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 45, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(back_btn, close_chapter_list, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    
    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 500);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 10, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 100);
    
    lv_obj_t *chapter_list = lv_list_create(list_container);
    lv_obj_set_size(chapter_list, 420, 480);
    lv_obj_align(chapter_list, LV_ALIGN_CENTER, 0, 0);
    
    for (int i = 0; i < chapter_count; i++) {
        lv_obj_t *btn = lv_btn_create(chapter_list);
        lv_obj_set_width(btn, LV_PCT(100));
        
        if (i == current_chapter) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a8a6a), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a45), 0);
        }
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, chapters[i].title);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        
        if (i == current_chapter) {
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_set_style_text_color(label, lv_color_hex(0xCCCCCC), 0);
        }
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, jump_to_chapter, LV_EVENT_CLICKED, NULL);
    }
    
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "%d chapters", chapter_count);
    lv_label_set_text(status_label, count_str);
}

static void search_input_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    const char *query = lv_textarea_get_text(ta);
    
    if (!query || strlen(query) < 2) {
        lv_obj_clean(search_results);
        return;
    }
    
    strncpy(search_query, query, sizeof(search_query) - 1);
    search_text(query);
    
    lv_obj_clean(search_results);
    
    for (int i = 0; i < search_result_count; i++) {
        lv_obj_t *btn = lv_btn_create(search_results);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a45), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, search_results_array[i].snippet);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, jump_to_search_result, LV_EVENT_CLICKED, NULL);
    }
}

static void jump_to_search_result(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    
    if (index < 0 || index >= search_result_count) return;
    
    current_page = search_results_array[index].page;
    render_page(current_page);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    
    char status[128];
    snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, status);
    
    create_reading_ui();
}

static void close_search(lv_event_t *e)
{
    (void)e;
    create_reading_ui();
}

static void show_search(lv_event_t *e)
{
    (void)e;
    
    lv_obj_clean(scr);
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "🔍 Search");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 45, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(back_btn, close_search, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    
    search_input = lv_textarea_create(scr);
    lv_obj_set_size(search_input, 400, 45);
    lv_obj_set_style_bg_color(search_input, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_color(search_input, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_border_width(search_input, 1, 0);
    lv_obj_set_style_radius(search_input, 8, 0);
    lv_obj_set_style_text_color(search_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(search_input, &lv_font_montserrat_14, 0);
    lv_textarea_set_placeholder_text(search_input, "Search in book...");
    lv_obj_align(search_input, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_add_event_cb(search_input, search_input_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 500);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 10, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 160);
    
    search_results = lv_list_create(list_container);
    lv_obj_set_size(search_results, 420, 480);
    lv_obj_align(search_results, LV_ALIGN_CENTER, 0, 0);
    
    current_mode = READER_MODE_SEARCH;
}

static void back_to_list(lv_event_t *e)
{
    (void)e;
    current_mode = READER_MODE_LIST;
    create_list_ui();
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

static void book_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    lv_obj_t *btn = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(btn);
    load_book(index);
}

static void create_list_ui(void)
{
    lv_obj_clean(scr);
    current_mode = READER_MODE_LIST;
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "📚 E-Reader");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Loading books...");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 80);
    
    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 450);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 10, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 110);
    
    book_list = lv_list_create(list_container);
    lv_obj_set_size(book_list, 420, 430);
    lv_obj_align(book_list, LV_ALIGN_CENTER, 0, 0);
    
    for (int i = 0; i < book_count; i++) {
        lv_obj_t *btn = lv_btn_create(book_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a45), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, books[i].title);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_t *format_label = lv_label_create(btn);
        lv_label_set_text(format_label, format_to_str(books[i].format));
        lv_obj_set_style_text_font(format_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(format_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_align(format_label, LV_ALIGN_RIGHT_MID, -10, 0);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, book_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "Books: %d", book_count);
    lv_label_set_text(status_label, count_str);
    
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
}

static void create_bookmark_ui(void)
{
    lv_obj_clean(scr);
    
    status_bar_create(scr);
    
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "🔖 Bookmarks");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);
    
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 45, 45);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 50);
    lv_obj_add_event_cb(back_btn, close_chapter_list, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "←");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_center(back_label);
    
    lv_obj_t *list_container = lv_obj_create(scr);
    lv_obj_set_size(list_container, 440, 500);
    lv_obj_set_style_bg_color(list_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(list_container, 1, 0);
    lv_obj_set_style_border_color(list_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(list_container, 10, 0);
    lv_obj_align(list_container, LV_ALIGN_TOP_MID, 0, 100);
    
    lv_obj_t *bookmark_list = lv_list_create(list_container);
    lv_obj_set_size(bookmark_list, 420, 480);
    lv_obj_align(bookmark_list, LV_ALIGN_CENTER, 0, 0);
    
    for (int i = 0; i < bookmark_count; i++) {
        lv_obj_t *btn = lv_btn_create(bookmark_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a45), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x353550), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        
        lv_obj_t *label = lv_label_create(btn);
        char text[192];
        snprintf(text, sizeof(text), "%s - Page %d/%d", 
                 bookmarks[i].title, bookmarks[i].page + 1, bookmarks[i].total_pages);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        
        lv_obj_t *del_btn = lv_btn_create(btn);
        lv_obj_set_size(del_btn, 35, 35);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0x6a3a3a), 0);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0x7a4a4a), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(del_btn, 0, 0);
        lv_obj_set_style_radius(del_btn, 6, 0);
        lv_obj_align(del_btn, LV_ALIGN_RIGHT_MID, -10, 0);
        
        lv_obj_t *del_label = lv_label_create(del_btn);
        lv_label_set_text(del_label, "✕");
        lv_obj_set_style_text_font(del_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(del_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(del_label);
        
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_set_user_data(del_btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, jump_to_bookmark, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(del_btn, remove_bookmark, LV_EVENT_CLICKED, NULL);
    }
    
    if (bookmark_count == 0) {
        lv_obj_t *empty_label = lv_label_create(list_container);
        lv_label_set_text(empty_label, "No bookmarks yet");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0x666666), 0);
        lv_obj_center(empty_label);
    }
    
    char count_str[64];
    snprintf(count_str, sizeof(count_str), "%d bookmarks", bookmark_count);
    lv_label_set_text(status_label, count_str);
    
    current_mode = READER_MODE_BOOKMARKS;
}

static void show_bookmarks(lv_event_t *e)
{
    (void)e;
    load_bookmarks();
    create_bookmark_ui();
}

static void create_reading_ui(void)
{
    lv_obj_clean(scr);
    current_mode = READER_MODE_READING;
    
    status_bar_create(scr);
    
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, books[current_book].title);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 50);
    
    page_label = lv_label_create(scr);
    char page_str[64];
    snprintf(page_str, sizeof(page_str), "%d/%d", current_page + 1, total_pages);
    lv_label_set_text(page_label, page_str);
    lv_obj_set_style_text_font(page_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(page_label, LV_ALIGN_TOP_RIGHT, -20, 50);
    
    lv_obj_t *text_container = lv_obj_create(scr);
    lv_obj_set_size(text_container, 460, 500);
    lv_obj_set_style_bg_color(text_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(text_container, 1, 0);
    lv_obj_set_style_border_color(text_container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(text_container, 10, 0);
    lv_obj_align(text_container, LV_ALIGN_TOP_MID, 0, 85);
    
    text_area = lv_label_create(text_container);
    lv_obj_set_size(text_area, 440, 480);
    lv_label_set_long_mode(text_area, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(text_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(text_area, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_line_space(text_area, font_size / 2, 0);
    lv_obj_align(text_area, LV_ALIGN_TOP_LEFT, 10, 10);
    
    progress_bar = lv_slider_create(scr);
    lv_obj_set_size(progress_bar, 440, 15);
    lv_slider_set_range(progress_bar, 0, total_pages - 1);
    lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x4a8a6a), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(progress_bar, 0, 0);
    lv_obj_align(progress_bar, LV_ALIGN_BOTTOM_MID, 0, -80);
    lv_obj_add_event_cb(progress_bar, page_changed, LV_EVENT_VALUE_CHANGED, NULL);
    
    speed_label = lv_label_create(scr);
    lv_label_set_text(speed_label, "Speed: 0");
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(speed_label, LV_ALIGN_BOTTOM_MID, 0, -55);
    
    lv_obj_t *nav_container = lv_obj_create(scr);
    lv_obj_set_size(nav_container, 460, 55);
    lv_obj_set_style_bg_color(nav_container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(nav_container, 0, 0);
    lv_obj_set_style_radius(nav_container, 8, 0);
    lv_obj_align(nav_container, LV_ALIGN_BOTTOM_MID, 0, -140);
    
    lv_obj_t *list_btn = create_icon_button(nav_container, "📖", back_to_list);
    lv_obj_align(list_btn, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t *chapter_btn = create_icon_button(nav_container, "📋", show_chapter_list);
    lv_obj_align(chapter_btn, LV_ALIGN_LEFT_MID, 65, 0);
    
    lv_obj_t *search_btn = create_icon_button(nav_container, "🔍", show_search);
    lv_obj_align(search_btn, LV_ALIGN_LEFT_MID, 120, 0);
    
    lv_obj_t *font_down_btn = create_icon_button(nav_container, "-A", decrease_font);
    lv_obj_align(font_down_btn, LV_ALIGN_LEFT_MID, 175, 0);
    
    lv_obj_t *font_up_btn = create_icon_button(nav_container, "+A", increase_font);
    lv_obj_align(font_up_btn, LV_ALIGN_LEFT_MID, 230, 0);
    
    lv_obj_t *prev_btn = create_icon_button(nav_container, "◀", prev_page);
    lv_obj_align(prev_btn, LV_ALIGN_LEFT_MID, 285, 0);
    
    lv_obj_t *play_btn = lv_btn_create(nav_container);
    lv_obj_set_size(play_btn, 55, 45);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x4a8a6a), 0);
    lv_obj_set_style_bg_color(play_btn, lv_color_hex(0x5a9a7a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_radius(play_btn, 8, 0);
    lv_obj_align(play_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(play_btn, toggle_auto_scroll, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "▶");
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(play_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_label);
    
    lv_obj_t *next_btn = create_icon_button(nav_container, "▶", next_page);
    lv_obj_align(next_btn, LV_ALIGN_RIGHT_MID, -120, 0);
    
    lv_obj_t *bookmark_btn = create_icon_button(nav_container, "🔖", add_bookmark);
    lv_obj_align(bookmark_btn, LV_ALIGN_RIGHT_MID, -65, 0);
    
    lv_obj_t *theme_btn = create_icon_button(nav_container, "🎨", toggle_theme);
    lv_obj_align(theme_btn, LV_ALIGN_RIGHT_MID, -10, 0);
}

static void auto_scroll_task(void *arg)
{
    (void)arg;
    
    while (1) {
        if (is_auto_scrolling && current_mode == READER_MODE_READING) {
            vTaskDelay(pdMS_TO_TICKS(2000 - scroll_speed * 150));
            
            if (current_page < total_pages - 1) {
                current_page++;
                render_page(current_page);
                lv_slider_set_value(progress_bar, current_page, LV_ANIM_OFF);
                update_reading_history();
                
                char status[128];
                snprintf(status, sizeof(status), "%d/%d", current_page + 1, total_pages);
                lv_label_set_text(page_label, status);
            } else {
                is_auto_scrolling = false;
                lv_label_set_text(speed_label, "Auto: Done");
            }
        }
        
        if (sleep_timer > 0) {
            vTaskDelay(pdMS_TO_TICKS(60000));
            sleep_timer--;
            if (sleep_timer == 0) {
                lv_label_set_text(speed_label, "Timer: Off");
                is_auto_scrolling = false;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
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

static void back_button_cb(lv_event_t *e)
{
    (void)e;
    if (book_content) {
        heap_caps_free(book_content);
        book_content = NULL;
    }
    app_manager_go_home();
}

lv_obj_t *reader_app_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    load_bookmarks();
    load_reading_history();
    init_audio();
    scan_books();

    xTaskCreate(auto_scroll_task, "auto_scroll", 2048, NULL, 5, NULL);

    create_list_ui();
    ui_animation_fade(scr, true, 300);

    ESP_LOGI(TAG, "Reader app created");
    return scr;
}