#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_CACHE_MAX_ENTRIES 16
#define FILE_CACHE_MAX_PATH_LEN 128

typedef enum {
    FILE_CACHE_POLICY_LRU,
    FILE_CACHE_POLICY_LFU,
    FILE_CACHE_POLICY_ADAPTIVE
} file_cache_policy_t;

typedef struct {
    char path[FILE_CACHE_MAX_PATH_LEN];
    uint8_t *data;
    size_t size;
    size_t access_count;
    uint32_t last_access_time;
    uint32_t creation_time;
    bool valid;
} file_cache_entry_t;

esp_err_t file_cache_init(size_t max_cache_size);

esp_err_t file_cache_set_policy(file_cache_policy_t policy);

file_cache_policy_t file_cache_get_policy(void);

void *file_cache_read(const char *path, size_t *out_size);

esp_err_t file_cache_write(const char *path, const uint8_t *data, size_t size);

void file_cache_invalidate(const char *path);

void file_cache_flush(void);

void file_cache_dump(void);

void file_cache_deinit(void);

size_t file_cache_get_used_size(void);

size_t file_cache_get_hits(void);

size_t file_cache_get_misses(void);

#ifdef __cplusplus
}
#endif