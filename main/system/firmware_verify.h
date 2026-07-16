#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_VERIFY_SHA256_SIZE 32
#define FIRMWARE_VERIFY_MAGIC "FWVER"
#define FIRMWARE_VERIFY_MAGIC_SIZE 4
#define FIRMWARE_VERIFY_VERSION 1

typedef struct {
    char magic[FIRMWARE_VERIFY_MAGIC_SIZE];
    uint32_t version;
    uint8_t sha256[FIRMWARE_VERIFY_SHA256_SIZE];
    uint32_t firmware_size;
    uint32_t timestamp;
    uint8_t reserved[128];
} firmware_verify_header_t;

typedef enum {
    FIRMWARE_STATUS_OK,
    FIRMWARE_STATUS_INVALID_HEADER,
    FIRMWARE_STATUS_HASH_MISMATCH,
    FIRMWARE_STATUS_TOO_LARGE,
    FIRMWARE_STATUS_CORRUPTED
} firmware_verify_status_t;

esp_err_t firmware_verify_init(void);
esp_err_t firmware_verify_deinit(void);

firmware_verify_status_t firmware_verify_check(const uint8_t *firmware_data, size_t firmware_size);
esp_err_t firmware_verify_generate_header(const uint8_t *firmware_data, size_t firmware_size, 
                                          firmware_verify_header_t *header);
bool firmware_verify_is_header_valid(const firmware_verify_header_t *header);
const char *firmware_verify_status_to_string(firmware_verify_status_t status);

#ifdef __cplusplus
}
#endif