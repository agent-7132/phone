#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t secure_storage_init(void);
esp_err_t secure_storage_set(const char *key, const char *value);
esp_err_t secure_storage_get(const char *key, char *value, size_t max_len);
esp_err_t secure_storage_delete(const char *key);
bool secure_storage_exists(const char *key);
esp_err_t secure_storage_clear_all(void);

#ifdef __cplusplus
}
#endif
