#include "memory_protection.h"
#include "esp_log.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"

static const char *TAG = "MEM_PROTECTION";

static mem_region_t regions[MEM_PROTECTION_REGION_COUNT];
static int region_count = 0;
static void (*fault_handler)(void *addr, bool write) = NULL;

static mem_stats_t s_stats = {0};
static uint32_t s_peak_allocated = 0;

#define HEAP_TRACE_BUFFER_SIZE 256
static heap_trace_record_t s_heap_trace_buffer[HEAP_TRACE_BUFFER_SIZE] = {0};

static int find_region_index(void *addr)
{
    for (int i = 0; i < region_count; i++) {
        if (regions[i].enabled && 
            addr >= regions[i].start_addr && 
            addr < (void*)((uint8_t*)regions[i].start_addr + regions[i].size)) {
            return i;
        }
    }
    return -1;
}

esp_err_t memory_protection_init(void)
{
    ESP_LOGI(TAG, "Initializing memory protection...");
    
    region_count = 0;
    memset(regions, 0, sizeof(regions));
    
    ESP_LOGI(TAG, "Memory protection initialized");
    return ESP_OK;
}

esp_err_t memory_protection_add_region(void *start_addr, size_t size, mem_region_type_t type)
{
    if (!start_addr || size == 0) {
        ESP_LOGE(TAG, "Invalid region parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (region_count >= MEM_PROTECTION_REGION_COUNT) {
        ESP_LOGE(TAG, "Maximum regions reached");
        return ESP_ERR_NO_MEM;
    }
    
    regions[region_count].start_addr = start_addr;
    regions[region_count].size = size;
    regions[region_count].type = type;
    regions[region_count].enabled = true;
    
    region_count++;
    
    ESP_LOGI(TAG, "Added memory region: 0x%p - 0x%p (%d bytes), type=%d", 
             start_addr, (void*)((uint8_t*)start_addr + size), size, type);
    
    return ESP_OK;
}

esp_err_t memory_protection_remove_region(void *start_addr)
{
    for (int i = 0; i < region_count; i++) {
        if (regions[i].start_addr == start_addr) {
            ESP_LOGI(TAG, "Removing memory region: 0x%p", start_addr);
            
            for (int j = i; j < region_count - 1; j++) {
                regions[j] = regions[j + 1];
            }
            region_count--;
            
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Region not found: 0x%p", start_addr);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t memory_protection_enable_region(void *start_addr)
{
    for (int i = 0; i < region_count; i++) {
        if (regions[i].start_addr == start_addr) {
            regions[i].enabled = true;
            ESP_LOGI(TAG, "Enabled memory region: 0x%p", start_addr);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Region not found: 0x%p", start_addr);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t memory_protection_disable_region(void *start_addr)
{
    for (int i = 0; i < region_count; i++) {
        if (regions[i].start_addr == start_addr) {
            regions[i].enabled = false;
            ESP_LOGI(TAG, "Disabled memory region: 0x%p", start_addr);
            return ESP_OK;
        }
    }
    
    ESP_LOGE(TAG, "Region not found: 0x%p", start_addr);
    return ESP_ERR_NOT_FOUND;
}

bool memory_protection_check_access(void *addr, size_t size, bool write)
{
    int index = find_region_index(addr);
    if (index < 0) {
        return true;
    }
    
    mem_region_t *region = &regions[index];
    
    switch (region->type) {
        case MEM_REGION_NONE:
            return true;
            
        case MEM_REGION_READ_ONLY:
            if (write) {
                ESP_LOGE(TAG, "Write access denied to read-only region: 0x%p", addr);
                if (fault_handler) {
                    fault_handler(addr, true);
                }
                return false;
            }
            return true;
            
        case MEM_REGION_READ_WRITE:
            return true;
            
        case MEM_REGION_NO_ACCESS:
            ESP_LOGE(TAG, "Access denied to no-access region: 0x%p", addr);
            if (fault_handler) {
                fault_handler(addr, write);
            }
            return false;
            
        default:
            return true;
    }
}

void memory_protection_register_fault_handler(void (*handler)(void *addr, bool write))
{
    fault_handler = handler;
    ESP_LOGI(TAG, "Fault handler registered");
}

esp_err_t memory_protection_get_stats(mem_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    stats->total_allocated = (total_internal - free_internal) + (total_psram - free_psram);
    stats->peak_allocated = s_peak_allocated;
    stats->alloc_count = s_stats.alloc_count;
    stats->free_count = s_stats.free_count;
    stats->leak_count = s_stats.alloc_count - s_stats.free_count;
    
    return ESP_OK;
}

void memory_protection_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(s_stats));
    s_peak_allocated = 0;
    ESP_LOGI(TAG, "Memory stats reset");
}

void *memory_protection_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
    if (ptr) {
        s_stats.alloc_count++;
        s_stats.total_allocated += size;
        if (s_stats.total_allocated > s_peak_allocated) {
            s_peak_allocated = s_stats.total_allocated;
        }
    }
    return ptr;
}

void *memory_protection_malloc_psram(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        s_stats.alloc_count++;
        s_stats.total_allocated += size;
        if (s_stats.total_allocated > s_peak_allocated) {
            s_peak_allocated = s_stats.total_allocated;
        }
    }
    return ptr;
}

void memory_protection_free(void *ptr)
{
    if (ptr) {
        s_stats.free_count++;
    }
    heap_caps_free(ptr);
}

void *memory_protection_calloc(size_t nmemb, size_t size)
{
    void *ptr = heap_caps_calloc(nmemb, size, MALLOC_CAP_INTERNAL);
    if (ptr) {
        s_stats.alloc_count++;
        s_stats.total_allocated += nmemb * size;
        if (s_stats.total_allocated > s_peak_allocated) {
            s_peak_allocated = s_stats.total_allocated;
        }
    }
    return ptr;
}

void *memory_protection_realloc(void *ptr, size_t size)
{
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL);
    if (new_ptr) {
        if (!ptr) {
            s_stats.alloc_count++;
            s_stats.total_allocated += size;
        }
        if (s_stats.total_allocated > s_peak_allocated) {
            s_peak_allocated = s_stats.total_allocated;
        }
    }
    return new_ptr;
}

void memory_protection_enable_leak_detection(bool enable)
{
    if (enable) {
        esp_err_t ret = heap_trace_init_standalone(s_heap_trace_buffer, HEAP_TRACE_BUFFER_SIZE);
        if (ret == ESP_OK) {
            ret = heap_trace_start(HEAP_TRACE_LEAKS);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Heap tracing leak detection enabled (buffer size: %d)", HEAP_TRACE_BUFFER_SIZE);
            } else {
                ESP_LOGE(TAG, "Failed to start heap tracing: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGE(TAG, "Failed to initialize heap tracing: %s", esp_err_to_name(ret));
        }
    } else {
        heap_trace_stop();
        heap_trace_init_standalone(NULL, 0);
        ESP_LOGI(TAG, "Heap tracing leak detection disabled");
    }
}

void memory_protection_dump_allocations(void)
{
    mem_stats_t stats;
    memory_protection_get_stats(&stats);
    
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    
    ESP_LOGI(TAG, "=== Memory Protection Dump ===");
    ESP_LOGI(TAG, "Regions registered: %d", region_count);
    ESP_LOGI(TAG, "Internal RAM: %lu/%lu bytes free", free_internal, total_internal);
    ESP_LOGI(TAG, "PSRAM: %lu/%lu bytes free", free_psram, total_psram);
    ESP_LOGI(TAG, "Total allocated: %lu bytes", stats.total_allocated);
    ESP_LOGI(TAG, "Peak allocated: %lu bytes", stats.peak_allocated);
    ESP_LOGI(TAG, "Alloc count: %lu", stats.alloc_count);
    ESP_LOGI(TAG, "Free count: %lu", stats.free_count);
    ESP_LOGI(TAG, "Potential leaks: %lu", stats.leak_count);
    
    for (int i = 0; i < region_count; i++) {
        const char *type_str = "";
        switch (regions[i].type) {
            case MEM_REGION_NONE: type_str = "NONE"; break;
            case MEM_REGION_READ_ONLY: type_str = "READ_ONLY"; break;
            case MEM_REGION_READ_WRITE: type_str = "READ_WRITE"; break;
            case MEM_REGION_NO_ACCESS: type_str = "NO_ACCESS"; break;
        }
        ESP_LOGI(TAG, "Region %d: 0x%p - 0x%p (%d bytes), type=%s, enabled=%d", 
                 i, regions[i].start_addr, 
                 (void*)((uint8_t*)regions[i].start_addr + regions[i].size), 
                 regions[i].size, type_str, regions[i].enabled);
    }
    
    ESP_LOGI(TAG, "=== End Memory Dump ===");
}

void memory_protection_dump_leaks(void)
{
    ESP_LOGI(TAG, "=== Memory Leak Dump ===");
    
    heap_trace_summary_t summary = {0};
    esp_err_t ret = heap_trace_summary(&summary);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Total allocations: %zu", summary.total_allocations);
        ESP_LOGI(TAG, "Total frees: %zu", summary.total_frees);
        ESP_LOGI(TAG, "Current record count: %zu", summary.count);
        ESP_LOGI(TAG, "Buffer capacity: %zu", summary.capacity);
        ESP_LOGI(TAG, "High water mark: %zu", summary.high_water_mark);
        ESP_LOGI(TAG, "Has overflowed: %zu", summary.has_overflowed);
    }
    
    size_t count = heap_trace_get_count();
    ESP_LOGI(TAG, "Leaked allocations: %zu", count);
    
    for (size_t i = 0; i < count; i++) {
        heap_trace_record_t rec = {0};
        ret = heap_trace_get(i, &rec);
        if (ret == ESP_OK && rec.address != NULL) {
            ESP_LOGI(TAG, "Leak %zu: addr=0x%p, size=%zu, freed=%s", 
                     i, rec.address, rec.size, rec.freed ? "yes" : "no");
            for (int j = 0; j < CONFIG_HEAP_TRACING_STACK_DEPTH; j++) {
                if (rec.alloced_by[j]) {
                    ESP_LOGI(TAG, "  Alloced by [%d]: 0x%p", j, rec.alloced_by[j]);
                }
            }
        }
    }
    
    ESP_LOGI(TAG, "=== End Leak Dump ===");
}