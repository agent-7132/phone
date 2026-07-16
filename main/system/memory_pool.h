#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMORY_POOL_MAX_POOLS 8
#define MEMORY_POOL_MAX_BLOCKS 256

typedef struct {
    void *buffer;
    size_t block_size;
    size_t block_count;
    bool *allocated;
    int free_count;
    char name[32];
} memory_pool_t;

esp_err_t memory_pool_init(void);

esp_err_t memory_pool_create(const char *name, size_t block_size, size_t block_count);

void *memory_pool_alloc(const char *name);

void memory_pool_free(const char *name, void *ptr);

void memory_pool_destroy(const char *name);

void memory_pool_dump(void);

size_t memory_pool_get_free_count(const char *name);

bool memory_pool_is_valid(const char *name);

#ifdef __cplusplus
}
#endif