#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_SIGNATURE_KEY_SIZE 256
#define APP_SIGNATURE_SHA256_SIZE 32
#define APP_SIGNATURE_RSA_SIZE 256

typedef enum {
    SIGNATURE_STATUS_OK,
    SIGNATURE_STATUS_INVALID,
    SIGNATURE_STATUS_NOT_FOUND,
    SIGNATURE_STATUS_VERIFY_FAILED,
    SIGNATURE_STATUS_KEY_MISMATCH
} app_signature_status_t;

typedef struct {
    uint8_t sha256[APP_SIGNATURE_SHA256_SIZE];
    uint8_t signature[APP_SIGNATURE_RSA_SIZE];
    uint32_t version;
    char author[64];
    uint32_t timestamp;
} app_signature_t;

esp_err_t app_signature_init(void);
esp_err_t app_signature_deinit(void);

app_signature_status_t app_signature_verify(const char *app_path, const uint8_t *public_key, size_t key_size);
esp_err_t app_signature_generate(const char *app_path, const uint8_t *private_key, size_t key_size, app_signature_t *signature);
esp_err_t app_signature_load(const char *app_path, app_signature_t *signature);
esp_err_t app_signature_save(const char *app_path, const app_signature_t *signature);

const char *app_signature_status_to_string(app_signature_status_t status);

#ifdef __cplusplus
}
#endif