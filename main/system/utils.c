#include "utils.h"
#include <stdlib.h>
#include <ctype.h>
#include "esp_heap_caps.h"

uint32_t utils_hash_string(const char *str)
{
    if (!str) return 0;
    
    uint32_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + tolower(c);
    }
    
    return hash % UTILS_HASH_TABLE_SIZE;
}

void utils_hash_table_init(hash_table_t *table)
{
    if (!table) return;
    
    memset(table, 0, sizeof(hash_table_t));
}

void utils_hash_table_insert(hash_table_t *table, const char *key, void *value)
{
    if (!table || !key) return;
    
    uint32_t hash = utils_hash_string(key);
    hash_node_t *node = (hash_node_t *)heap_caps_malloc(sizeof(hash_node_t), MALLOC_CAP_SPIRAM);
    
    if (!node) return;
    
    node->key = utils_strdup(key);
    node->value = value;
    node->next = table->buckets[hash];
    table->buckets[hash] = node;
    table->count++;
}

void *utils_hash_table_lookup(hash_table_t *table, const char *key)
{
    if (!table || !key) return NULL;
    
    uint32_t hash = utils_hash_string(key);
    hash_node_t *node = table->buckets[hash];
    
    while (node) {
        if (utils_strcasecmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }
    
    return NULL;
}

bool utils_hash_table_remove(hash_table_t *table, const char *key)
{
    if (!table || !key) return false;
    
    uint32_t hash = utils_hash_string(key);
    hash_node_t *node = table->buckets[hash];
    hash_node_t *prev = NULL;
    
    while (node) {
        if (utils_strcasecmp(node->key, key) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                table->buckets[hash] = node->next;
            }
            
            heap_caps_free((void *)node->key);
            heap_caps_free(node);
            table->count--;
            return true;
        }
        
        prev = node;
        node = node->next;
    }
    
    return false;
}

void utils_hash_table_clear(hash_table_t *table)
{
    if (!table) return;
    
    for (int i = 0; i < UTILS_HASH_TABLE_SIZE; i++) {
        hash_node_t *node = table->buckets[i];
        while (node) {
            hash_node_t *next = node->next;
            heap_caps_free((void *)node->key);
            heap_caps_free(node);
            node = next;
        }
        table->buckets[i] = NULL;
    }
    
    table->count = 0;
}

int utils_hash_table_count(hash_table_t *table)
{
    return table ? table->count : 0;
}

int utils_strcasecmp(const char *s1, const char *s2)
{
    if (!s1 || !s2) {
        return s1 ? 1 : (s2 ? -1 : 0);
    }
    
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) {
            return diff;
        }
        s1++;
        s2++;
    }
    
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

char *utils_strcasestr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    
    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    
    if (needle_len == 0) return (char *)haystack;
    if (haystack_len < needle_len) return NULL;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j;
        for (j = 0; j < needle_len; j++) {
            if (toupper((unsigned char)haystack[i + j]) != toupper((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return (char *)(haystack + i);
        }
    }
    
    return NULL;
}

char *utils_strdup(const char *str)
{
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char *copy = (char *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    
    if (copy) {
        memcpy(copy, str, len);
    }
    
    return copy;
}

static int cmp_str(const void *a, const void *b)
{
    return utils_strcasecmp(*(const char **)a, *(const char **)b);
}

void utils_sort_string_array(char **array, int count)
{
    if (!array || count <= 1) return;
    
    qsort(array, count, sizeof(char *), cmp_str);
}

int utils_binary_search_string(char **array, int count, const char *key)
{
    if (!array || !key || count <= 0) return -1;
    
    int left = 0;
    int right = count - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = utils_strcasecmp(array[mid], key);
        
        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;
}