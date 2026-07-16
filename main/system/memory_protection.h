#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_PROTECTION_REGION_COUNT 8

typedef enum {
    MEM_REGION_NONE,
    MEM_REGION_READ_ONLY,
    MEM_REGION_READ_WRITE,
    MEM_REGION_NO_ACCESS
} mem_region_type_t;

typedef struct {
    void *start_addr;
    size_t size;
    mem_region_type_t type;
    bool enabled;
} mem_region_t;

typedef struct {
    uint32_t total_allocated;
    uint32_t peak_allocated;
    uint32_t alloc_count;
    uint32_t free_count;
    uint32_t leak_count;
} mem_stats_t;

esp_err_t memory_protection_init(void);

esp_err_t memory_protection_add_region(void *start_addr, size_t size, mem_region_type_t type);

esp_err_t memory_protection_remove_region(void *start_addr);

esp_err_t memory_protection_enable_region(void *start_addr);

esp_err_t memory_protection_disable_region(void *start_addr);

bool memory_protection_check_access(void *addr, size_t size, bool write);

void memory_protection_register_fault_handler(void (*handler)(void *addr, bool write));

esp_err_t memory_protection_get_stats(mem_stats_t *stats);

void memory_protection_reset_stats(void);

void *memory_protection_malloc(size_t size);

void *memory_protection_malloc_psram(size_t size);

void memory_protection_free(void *ptr);

void *memory_protection_calloc(size_t nmemb, size_t size);

void *memory_protection_realloc(void *ptr, size_t size);

void memory_protection_enable_leak_detection(bool enable);

void memory_protection_dump_allocations(void);

void memory_protection_dump_leaks(void);

#ifdef __cplusplus
}
#endif