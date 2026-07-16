#include "file_cache.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "FILE_CACHE";

static file_cache_entry_t s_cache[FILE_CACHE_MAX_ENTRIES] = {0};
static size_t s_max_cache_size = 0;
static size_t s_current_cache_size = 0;
static size_t s_hits = 0;
static size_t s_misses = 0;

static int find_entry(const char *path)
{
    for (int i = 0; i < FILE_CACHE_MAX_ENTRIES; i++) {
        if (s_cache[i].valid && strcmp(s_cache[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_empty_entry(void)
{
    for (int i = 0; i < FILE_CACHE_MAX_ENTRIES; i++) {
        if (!s_cache[i].valid) {
            return i;
        }
    }
    return -1;
}

static int find_lru_entry(void)
{
    int lru_index = 0;
    uint32_t oldest_time = s_cache[0].last_access_time;
    
    for (int i = 1; i < FILE_CACHE_MAX_ENTRIES; i++) {
        if (s_cache[i].valid && s_cache[i].last_access_time < oldest_time) {
            oldest_time = s_cache[i].last_access_time;
            lru_index = i;
        }
    }
    
    return lru_index;
}

static void free_entry(int index)
{
    if (s_cache[index].valid && s_cache[index].data) {
        heap_caps_free(s_cache[index].data);
        s_current_cache_size -= s_cache[index].size;
        s_cache[index].valid = false;
        s_cache[index].data = NULL;
        s_cache[index].size = 0;
        s_cache[index].access_count = 0;
    }
}

static esp_err_t evict_entries(size_t required_size)
{
    while (s_current_cache_size + required_size > s_max_cache_size) {
        int lru_index = find_lru_entry();
        if (lru_index < 0) {
            ESP_LOGE(TAG, "No cache entry to evict");
            return ESP_ERR_NO_MEM;
        }
        
        ESP_LOGD(TAG, "Evicting cache entry: %s (size: %zu)", s_cache[lru_index].path, s_cache[lru_index].size);
        free_entry(lru_index);
    }
    
    return ESP_OK;
}

esp_err_t file_cache_init(size_t max_cache_size)
{
    ESP_LOGI(TAG, "Initializing file cache with max size: %zu bytes", max_cache_size);
    
    s_max_cache_size = max_cache_size;
    s_current_cache_size = 0;
    s_hits = 0;
    s_misses = 0;
    
    memset(s_cache, 0, sizeof(s_cache));
    
    ESP_LOGI(TAG, "File cache initialized");
    return ESP_OK;
}

void *file_cache_read(const char *path, size_t *out_size)
{
    if (!path || !out_size) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }
    
    int index = find_entry(path);
    if (index >= 0) {
        s_cache[index].access_count++;
        s_cache[index].last_access_time = esp_timer_get_time();
        *out_size = s_cache[index].size;
        s_hits++;
        ESP_LOGD(TAG, "Cache hit: %s (access_count: %zu)", path, s_cache[index].access_count);
        return s_cache[index].data;
    }
    
    s_misses++;
    ESP_LOGD(TAG, "Cache miss: %s", path);
    
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (file_size == 0) {
        fclose(fp);
        ESP_LOGW(TAG, "Empty file: %s", path);
        *out_size = 0;
        return NULL;
    }
    
    if (file_size > s_max_cache_size) {
        fclose(fp);
        ESP_LOGW(TAG, "File size %zu exceeds max cache size %zu, not caching: %s", 
                 file_size, s_max_cache_size, path);
        
        uint8_t *data = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
        if (!data) {
            ESP_LOGE(TAG, "Failed to allocate buffer for file: %s", path);
            return NULL;
        }
        
        fp = fopen(path, "rb");
        size_t bytes_read = fread(data, 1, file_size, fp);
        fclose(fp);
        
        if (bytes_read != file_size) {
            heap_caps_free(data);
            ESP_LOGE(TAG, "Failed to read complete file: %s", path);
            return NULL;
        }
        
        *out_size = file_size;
        return data;
    }
    
    esp_err_t ret = evict_entries(file_size);
    if (ret != ESP_OK) {
        fclose(fp);
        ESP_LOGE(TAG, "Failed to evict cache entries");
        return NULL;
    }
    
    int empty_index = find_empty_entry();
    if (empty_index < 0) {
        fclose(fp);
        ESP_LOGE(TAG, "No empty cache entry available");
        return NULL;
    }
    
    uint8_t *data = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!data) {
        fclose(fp);
        ESP_LOGE(TAG, "Failed to allocate cache buffer from PSRAM");
        return NULL;
    }
    
    size_t bytes_read = fread(data, 1, file_size, fp);
    fclose(fp);
    
    if (bytes_read != file_size) {
        heap_caps_free(data);
        ESP_LOGE(TAG, "Failed to read complete file: %s", path);
        return NULL;
    }
    
    strncpy(s_cache[empty_index].path, path, sizeof(s_cache[empty_index].path) - 1);
    s_cache[empty_index].data = data;
    s_cache[empty_index].size = file_size;
    s_cache[empty_index].access_count = 1;
    s_cache[empty_index].last_access_time = esp_timer_get_time();
    s_cache[empty_index].valid = true;
    s_current_cache_size += file_size;
    
    *out_size = file_size;
    ESP_LOGI(TAG, "Cached file: %s (size: %zu, cache_used: %zu/%zu)", 
             path, file_size, s_current_cache_size, s_max_cache_size);
    
    return data;
}

esp_err_t file_cache_write(const char *path, const uint8_t *data, size_t size)
{
    if (!path || !data || size == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    file_cache_invalidate(path);
    
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }
    
    size_t bytes_written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    if (bytes_written != size) {
        ESP_LOGE(TAG, "Failed to write complete file: %s", path);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Written file: %s (size: %zu)", path, size);
    return ESP_OK;
}

void file_cache_invalidate(const char *path)
{
    if (!path) return;
    
    int index = find_entry(path);
    if (index >= 0) {
        ESP_LOGD(TAG, "Invalidating cache entry: %s", path);
        free_entry(index);
    }
}

void file_cache_flush(void)
{
    ESP_LOGI(TAG, "Flushing all cache entries");
    
    for (int i = 0; i < FILE_CACHE_MAX_ENTRIES; i++) {
        if (s_cache[i].valid) {
            free_entry(i);
        }
    }
    
    s_current_cache_size = 0;
    ESP_LOGI(TAG, "Cache flushed, used size: %zu", s_current_cache_size);
}

void file_cache_dump(void)
{
    ESP_LOGI(TAG, "=== File Cache Dump ===");
    ESP_LOGI(TAG, "Max cache size: %zu bytes", s_max_cache_size);
    ESP_LOGI(TAG, "Current cache size: %zu bytes", s_current_cache_size);
    ESP_LOGI(TAG, "Cache hits: %zu", s_hits);
    ESP_LOGI(TAG, "Cache misses: %zu", s_misses);
    ESP_LOGI(TAG, "Cache hit rate: %.1f%%", s_hits + s_misses > 0 ? 
             (float)s_hits / (s_hits + s_misses) * 100 : 0);
    
    for (int i = 0; i < FILE_CACHE_MAX_ENTRIES; i++) {
        if (s_cache[i].valid) {
            ESP_LOGI(TAG, "Entry %d: %s (size: %zu, access_count: %zu)",
                     i, s_cache[i].path, s_cache[i].size, s_cache[i].access_count);
        }
    }
    
    ESP_LOGI(TAG, "=== End File Cache Dump ===");
}

void file_cache_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing file cache");
    file_cache_flush();
}

size_t file_cache_get_used_size(void)
{
    return s_current_cache_size;
}

size_t file_cache_get_hits(void)
{
    return s_hits;
}

size_t file_cache_get_misses(void)
{
    return s_misses;
}