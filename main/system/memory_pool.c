#include "memory_pool.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include <string.h>
#include <stddef.h>

static const char *TAG = "MEMORY_POOL";

static memory_pool_t s_pools[MEMORY_POOL_MAX_POOLS] = {0};
static int s_pool_count = 0;

esp_err_t memory_pool_init(void)
{
    ESP_LOGI(TAG, "Initializing memory pool manager...");
    s_pool_count = 0;
    memset(s_pools, 0, sizeof(s_pools));
    ESP_LOGI(TAG, "Memory pool manager initialized");
    return ESP_OK;
}

static int find_pool_index(const char *name)
{
    for (int i = 0; i < s_pool_count; i++) {
        if (strcmp(s_pools[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static esp_err_t memory_pool_create_internal(const char *name, size_t block_size, 
                                             size_t block_count, bool dma_compatible)
{
    if (!name || block_size == 0 || block_count == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (s_pool_count >= MEMORY_POOL_MAX_POOLS) {
        ESP_LOGE(TAG, "Maximum pools reached (%d)", MEMORY_POOL_MAX_POOLS);
        return ESP_ERR_NO_MEM;
    }

    if (find_pool_index(name) >= 0) {
        ESP_LOGE(TAG, "Pool '%s' already exists", name);
        return ESP_ERR_INVALID_STATE;
    }

    if (block_count > MEMORY_POOL_MAX_BLOCKS) {
        ESP_LOGE(TAG, "Block count %zu exceeds maximum %d", block_count, MEMORY_POOL_MAX_BLOCKS);
        return ESP_ERR_INVALID_ARG;
    }

    size_t aligned_block_size = block_size;
    if (dma_compatible) {
        aligned_block_size = ((block_size + MEMORY_POOL_DMA_ALIGN - 1) / MEMORY_POOL_DMA_ALIGN) * MEMORY_POOL_DMA_ALIGN;
    }
    
    size_t total_size = aligned_block_size * block_count;
    uint32_t caps = dma_compatible ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA) : MALLOC_CAP_SPIRAM;
    void *buffer = heap_caps_malloc(total_size, caps);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes from PSRAM for pool '%s' (dma=%d)", 
                 total_size, name, dma_compatible);
        return ESP_ERR_NO_MEM;
    }

    if (dma_compatible && ((uintptr_t)buffer % MEMORY_POOL_DMA_ALIGN != 0)) {
        ESP_LOGE(TAG, "DMA buffer not properly aligned: 0x%p", buffer);
        heap_caps_free(buffer);
        return ESP_ERR_INVALID_STATE;
    }

    bool *allocated = heap_caps_malloc(block_count * sizeof(bool), MALLOC_CAP_SPIRAM);
    if (!allocated) {
        heap_caps_free(buffer);
        ESP_LOGE(TAG, "Failed to allocate allocation tracking array");
        return ESP_ERR_NO_MEM;
    }
    memset(allocated, 0, block_count * sizeof(bool));

    strncpy(s_pools[s_pool_count].name, name, sizeof(s_pools[s_pool_count].name) - 1);
    s_pools[s_pool_count].buffer = buffer;
    s_pools[s_pool_count].block_size = aligned_block_size;
    s_pools[s_pool_count].block_count = block_count;
    s_pools[s_pool_count].allocated = allocated;
    s_pools[s_pool_count].free_count = block_count;
    s_pools[s_pool_count].is_dma_compatible = dma_compatible;

    s_pool_count++;

    ESP_LOGI(TAG, "Created pool '%s': %zu blocks x %zu bytes = %zu bytes (PSRAM, DMA=%d)", 
             name, block_count, aligned_block_size, total_size, dma_compatible);

    return ESP_OK;
}

esp_err_t memory_pool_create(const char *name, size_t block_size, size_t block_count)
{
    return memory_pool_create_internal(name, block_size, block_count, false);
}

esp_err_t memory_pool_create_dma(const char *name, size_t block_size, size_t block_count)
{
    return memory_pool_create_internal(name, block_size, block_count, true);
}

void *memory_pool_alloc(const char *name)
{
    int index = find_pool_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Pool '%s' not found", name);
        return NULL;
    }

    memory_pool_t *pool = &s_pools[index];

    for (size_t i = 0; i < pool->block_count; i++) {
        if (!pool->allocated[i]) {
            pool->allocated[i] = true;
            pool->free_count--;
            void *ptr = (uint8_t *)pool->buffer + (i * pool->block_size);
            memset(ptr, 0, pool->block_size);
            return ptr;
        }
    }

    ESP_LOGW(TAG, "Pool '%s' is full", name);
    return NULL;
}

void memory_pool_free(const char *name, void *ptr)
{
    if (!ptr) return;

    int index = find_pool_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Pool '%s' not found", name);
        return;
    }

    memory_pool_t *pool = &s_pools[index];

    uintptr_t ptr_addr = (uintptr_t)ptr;
    uintptr_t buf_addr = (uintptr_t)pool->buffer;
    uintptr_t buf_end = buf_addr + (pool->block_size * pool->block_count);

    if (ptr_addr < buf_addr || ptr_addr >= buf_end) {
        ESP_LOGE(TAG, "Pointer 0x%p not in pool '%s' range [0x%p, 0x%p]", 
                 ptr, name, (void *)buf_addr, (void *)buf_end);
        return;
    }

    size_t block_index = (ptr_addr - buf_addr) / pool->block_size;
    if (block_index >= pool->block_count) {
        ESP_LOGE(TAG, "Invalid block index %zu for pool '%s'", block_index, name);
        return;
    }

    if (!pool->allocated[block_index]) {
        ESP_LOGE(TAG, "Double free detected for pointer 0x%p in pool '%s'", ptr, name);
        return;
    }

    pool->allocated[block_index] = false;
    pool->free_count++;
}

void memory_pool_destroy(const char *name)
{
    int index = find_pool_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Pool '%s' not found", name);
        return;
    }

    memory_pool_t *pool = &s_pools[index];

    heap_caps_free(pool->allocated);
    heap_caps_free(pool->buffer);

    for (int i = index; i < s_pool_count - 1; i++) {
        s_pools[i] = s_pools[i + 1];
    }
    s_pool_count--;

    ESP_LOGI(TAG, "Destroyed pool '%s'", name);
}

void memory_pool_dump(void)
{
    ESP_LOGI(TAG, "=== Memory Pool Dump ===");
    ESP_LOGI(TAG, "Total pools: %d", s_pool_count);

    for (int i = 0; i < s_pool_count; i++) {
        memory_pool_t *pool = &s_pools[i];
        float usage = ((float)(pool->block_count - pool->free_count) / pool->block_count) * 100;
        ESP_LOGI(TAG, "Pool '%s': %zu/%zu blocks used (%.1f%%), block_size=%zu bytes, DMA=%s",
                 pool->name, pool->block_count - pool->free_count, pool->block_count, 
                 usage, pool->block_size, pool->is_dma_compatible ? "yes" : "no");
    }

    ESP_LOGI(TAG, "=== End Memory Pool Dump ===");
}

size_t memory_pool_get_free_count(const char *name)
{
    int index = find_pool_index(name);
    if (index < 0) {
        return 0;
    }
    return s_pools[index].free_count;
}

bool memory_pool_is_valid(const char *name)
{
    return find_pool_index(name) >= 0;
}

bool memory_pool_is_dma_compatible(const char *name)
{
    int index = find_pool_index(name);
    if (index < 0) {
        return false;
    }
    return s_pools[index].is_dma_compatible;
}

void memory_pool_cache_sync_before_dma(const char *name, void *ptr, size_t size)
{
    int index = find_pool_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Pool '%s' not found", name);
        return;
    }

    memory_pool_t *pool = &s_pools[index];
    if (!pool->is_dma_compatible) {
        ESP_LOGW(TAG, "Pool '%s' is not DMA compatible", name);
        return;
    }

    esp_err_t ret = esp_cache_msync(ptr, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sync cache before DMA: %s", esp_err_to_name(ret));
    }
}

void memory_pool_cache_sync_after_dma(const char *name, void *ptr, size_t size)
{
    int index = find_pool_index(name);
    if (index < 0) {
        ESP_LOGE(TAG, "Pool '%s' not found", name);
        return;
    }

    memory_pool_t *pool = &s_pools[index];
    if (!pool->is_dma_compatible) {
        ESP_LOGW(TAG, "Pool '%s' is not DMA compatible", name);
        return;
    }

    esp_err_t ret = esp_cache_msync(ptr, size, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sync cache after DMA: %s", esp_err_to_name(ret));
    }
}