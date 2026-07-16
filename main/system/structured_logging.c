#include "structured_logging.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

static const char *TAG = "STRUCTURED_LOG";

#define MAX_LOG_ENTRIES 100

static struct_log_entry_t *log_buffer = NULL;
static int log_head = 0;
static int log_count = 0;
static int log_level = STRUCT_LOG_LEVEL_DEBUG;
static struct_log_output_cb_t output_cb = NULL;

static const char *level_to_str(int level)
{
    switch (level) {
        case STRUCT_LOG_LEVEL_DEBUG: return "DEBUG";
        case STRUCT_LOG_LEVEL_INFO: return "INFO";
        case STRUCT_LOG_LEVEL_WARN: return "WARN";
        case STRUCT_LOG_LEVEL_ERROR: return "ERROR";
        case STRUCT_LOG_LEVEL_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

esp_err_t struct_logging_init(void)
{
    ESP_LOGI(TAG, "Initializing structured logging...");
    
    if (log_buffer == NULL) {
        log_buffer = heap_caps_malloc(MAX_LOG_ENTRIES * sizeof(struct_log_entry_t), MALLOC_CAP_SPIRAM);
        if (log_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate log buffer from PSRAM");
            return ESP_ERR_NO_MEM;
        }
    }
    
    log_head = 0;
    log_count = 0;
    log_level = STRUCT_LOG_LEVEL_DEBUG;
    output_cb = NULL;
    memset(log_buffer, 0, MAX_LOG_ENTRIES * sizeof(struct_log_entry_t));
    
    ESP_LOGI(TAG, "Structured logging initialized (PSRAM buffer)");
    return ESP_OK;
}

void struct_logging_deinit(void)
{
    if (log_buffer != NULL) {
        heap_caps_free(log_buffer);
        log_buffer = NULL;
    }
    ESP_LOGI(TAG, "Structured logging deinitialized");
}

void struct_logging_set_level(int level)
{
    if (level >= STRUCT_LOG_LEVEL_DEBUG && level <= STRUCT_LOG_LEVEL_CRITICAL) {
        log_level = level;
        ESP_LOGI(TAG, "Log level set to: %s", level_to_str(level));
    }
}

int struct_logging_get_level(void)
{
    return log_level;
}

void struct_logging_add_field(const char *key, const char *value)
{
    struct_log_entry_t *entry = &log_buffer[log_head];
    
    if (entry->field_count < STRUCT_LOG_MAX_FIELDS) {
        strncpy(entry->fields[entry->field_count].key, key, 31);
        entry->fields[entry->field_count].key[31] = '\0';
        
        if (value) {
            strncpy(entry->fields[entry->field_count].value, value, 63);
            entry->fields[entry->field_count].value[63] = '\0';
        }
        
        entry->field_count++;
    }
}

void struct_logging_clear_fields(void)
{
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->field_count = 0;
}

void struct_logging_log(int level, const char *tag, const char *format, ...)
{
    if (level < log_level) {
        return;
    }
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = level;
    
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    
    va_list args;
    va_start(args, format);
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    va_end(args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOG_LEVEL_LOCAL(level, tag, "%s", entry->message);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

void struct_logging_debug(const char *tag, const char *format, ...)
{
    if (STRUCT_LOG_LEVEL_DEBUG < log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = STRUCT_LOG_LEVEL_DEBUG;
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOGD(tag, "%s", entry->message);
    
    va_end(args);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

void struct_logging_info(const char *tag, const char *format, ...)
{
    if (STRUCT_LOG_LEVEL_INFO < log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = STRUCT_LOG_LEVEL_INFO;
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOGI(tag, "%s", entry->message);
    
    va_end(args);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

void struct_logging_warn(const char *tag, const char *format, ...)
{
    if (STRUCT_LOG_LEVEL_WARN < log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = STRUCT_LOG_LEVEL_WARN;
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOGW(tag, "%s", entry->message);
    
    va_end(args);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

void struct_logging_error(const char *tag, const char *format, ...)
{
    if (STRUCT_LOG_LEVEL_ERROR < log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = STRUCT_LOG_LEVEL_ERROR;
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOGE(tag, "%s", entry->message);
    
    va_end(args);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

void struct_logging_critical(const char *tag, const char *format, ...)
{
    if (STRUCT_LOG_LEVEL_CRITICAL < log_level) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    struct_log_entry_t *entry = &log_buffer[log_head];
    entry->timestamp = esp_timer_get_time() / 1000;
    entry->level = STRUCT_LOG_LEVEL_CRITICAL;
    strncpy(entry->tag, tag ? tag : "UNKNOWN", STRUCT_LOG_MAX_TAG_LEN - 1);
    entry->tag[STRUCT_LOG_MAX_TAG_LEN - 1] = '\0';
    vsnprintf(entry->message, STRUCT_LOG_MAX_MSG_LEN, format, args);
    
    if (output_cb) {
        output_cb(entry);
    }
    
    ESP_LOGE(tag, "[CRITICAL] %s", entry->message);
    
    va_end(args);
    
    log_head = (log_head + 1) % MAX_LOG_ENTRIES;
    if (log_count < MAX_LOG_ENTRIES) {
        log_count++;
    }
}

esp_err_t struct_logging_register_output(struct_log_output_cb_t cb)
{
    output_cb = cb;
    ESP_LOGI(TAG, "Output callback registered");
    return ESP_OK;
}

esp_err_t struct_logging_unregister_output(struct_log_output_cb_t cb)
{
    if (output_cb == cb) {
        output_cb = NULL;
        ESP_LOGI(TAG, "Output callback unregistered");
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t struct_logging_save_to_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < log_count; i++) {
        int index = (log_head - log_count + i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        struct_log_entry_t *entry = &log_buffer[index];
        
        fprintf(fp, "[%" PRIu32 "] [%s] [%s] %s\n", 
                entry->timestamp, 
                level_to_str(entry->level),
                entry->tag,
                entry->message);
        
        for (int j = 0; j < entry->field_count; j++) {
            fprintf(fp, "  %s=%s\n", 
                    entry->fields[j].key,
                    entry->fields[j].value);
        }
    }
    
    fclose(fp);
    
    ESP_LOGI(TAG, "Logs saved to file: %s", path);
    
    return ESP_OK;
}

esp_err_t struct_logging_clear_logs(void)
{
    log_head = 0;
    log_count = 0;
    if (log_buffer) {
        memset(log_buffer, 0, MAX_LOG_ENTRIES * sizeof(struct_log_entry_t));
    }
    
    ESP_LOGI(TAG, "Logs cleared");
    
    return ESP_OK;
}

int struct_logging_get_entry_count(void)
{
    return log_count;
}

esp_err_t struct_logging_get_entries(struct_log_entry_t *entries, int max_count)
{
    if (!entries || max_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int count = (max_count < log_count) ? max_count : log_count;
    
    for (int i = 0; i < count; i++) {
        int index = (log_head - log_count + i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        entries[i] = log_buffer[index];
    }
    
    return ESP_OK;
}