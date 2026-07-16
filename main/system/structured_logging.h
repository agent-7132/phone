#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STRUCT_LOG_LEVEL_DEBUG 0
#define STRUCT_LOG_LEVEL_INFO 1
#define STRUCT_LOG_LEVEL_WARN 2
#define STRUCT_LOG_LEVEL_ERROR 3
#define STRUCT_LOG_LEVEL_CRITICAL 4

#define STRUCT_LOG_MAX_TAG_LEN 32
#define STRUCT_LOG_MAX_MSG_LEN 512
#define STRUCT_LOG_MAX_FIELDS 16

typedef struct {
    char key[32];
    char value[64];
} struct_log_field_t;

typedef struct {
    uint32_t timestamp;
    int level;
    char tag[STRUCT_LOG_MAX_TAG_LEN];
    char message[STRUCT_LOG_MAX_MSG_LEN];
    int field_count;
    struct_log_field_t fields[STRUCT_LOG_MAX_FIELDS];
} struct_log_entry_t;

typedef void (*struct_log_output_cb_t)(struct_log_entry_t *entry);

esp_err_t struct_logging_init(void);

void struct_logging_deinit(void);

void struct_logging_set_level(int level);

int struct_logging_get_level(void);

void struct_logging_add_field(const char *key, const char *value);

void struct_logging_clear_fields(void);

void struct_logging_log(int level, const char *tag, const char *format, ...);

void struct_logging_debug(const char *tag, const char *format, ...);

void struct_logging_info(const char *tag, const char *format, ...);

void struct_logging_warn(const char *tag, const char *format, ...);

void struct_logging_error(const char *tag, const char *format, ...);

void struct_logging_critical(const char *tag, const char *format, ...);

esp_err_t struct_logging_register_output(struct_log_output_cb_t cb);

esp_err_t struct_logging_unregister_output(struct_log_output_cb_t cb);

esp_err_t struct_logging_save_to_file(const char *path);

esp_err_t struct_logging_clear_logs(void);

int struct_logging_get_entry_count(void);

esp_err_t struct_logging_get_entries(struct_log_entry_t *entries, int max_count);

#ifdef __cplusplus
}
#endif