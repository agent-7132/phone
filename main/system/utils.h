#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UTILS_HASH_TABLE_SIZE 31

typedef struct hash_node {
    const char *key;
    void *value;
    struct hash_node *next;
} hash_node_t;

typedef struct {
    hash_node_t *buckets[UTILS_HASH_TABLE_SIZE];
    int count;
} hash_table_t;

uint32_t utils_hash_string(const char *str);
void utils_hash_table_init(hash_table_t *table);
void utils_hash_table_insert(hash_table_t *table, const char *key, void *value);
void *utils_hash_table_lookup(hash_table_t *table, const char *key);
bool utils_hash_table_remove(hash_table_t *table, const char *key);
void utils_hash_table_clear(hash_table_t *table);
int utils_hash_table_count(hash_table_t *table);

int utils_strcasecmp(const char *s1, const char *s2);
char *utils_strcasestr(const char *haystack, const char *needle);
char *utils_strdup(const char *str);

void utils_sort_string_array(char **array, int count);
int utils_binary_search_string(char **array, int count, const char *key);

#ifdef __cplusplus
}
#endif