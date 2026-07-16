#include "firmware_verify.h"
#include "esp_log.h"
#include "sha/sha_core.h"
#include "hal/sha_types.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "FIRMWARE_VERIFY";

esp_err_t firmware_verify_init(void)
{
    ESP_LOGI(TAG, "Initializing firmware verification module");
    return ESP_OK;
}

esp_err_t firmware_verify_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing firmware verification module");
    return ESP_OK;
}

bool firmware_verify_is_header_valid(const firmware_verify_header_t *header)
{
    if (!header) return false;

    if (memcmp(header->magic, FIRMWARE_VERIFY_MAGIC, FIRMWARE_VERIFY_MAGIC_SIZE) != 0) {
        return false;
    }

    if (header->version != FIRMWARE_VERIFY_VERSION) {
        ESP_LOGW(TAG, "Header version mismatch: %d != %d", header->version, FIRMWARE_VERIFY_VERSION);
        return false;
    }

    return true;
}

firmware_verify_status_t firmware_verify_check(const uint8_t *firmware_data, size_t firmware_size)
{
    if (!firmware_data || firmware_size == 0) {
        return FIRMWARE_STATUS_INVALID_HEADER;
    }

    if (firmware_size > 2 * 1024 * 1024) {
        return FIRMWARE_STATUS_TOO_LARGE;
    }

    firmware_verify_header_t header;
    if (firmware_size < sizeof(firmware_verify_header_t)) {
        return FIRMWARE_STATUS_CORRUPTED;
    }

    memcpy(&header, firmware_data, sizeof(firmware_verify_header_t));

    if (!firmware_verify_is_header_valid(&header)) {
        return FIRMWARE_STATUS_INVALID_HEADER;
    }

    if (header.firmware_size != firmware_size) {
        ESP_LOGE(TAG, "Firmware size mismatch: header=%u, actual=%u", header.firmware_size, (unsigned int)firmware_size);
        return FIRMWARE_STATUS_INVALID_HEADER;
    }

    uint8_t hash[FIRMWARE_VERIFY_SHA256_SIZE];
    esp_sha(SHA2_256, firmware_data + sizeof(firmware_verify_header_t), 
            firmware_size - sizeof(firmware_verify_header_t), hash);

    if (memcmp(hash, header.sha256, FIRMWARE_VERIFY_SHA256_SIZE) != 0) {
        ESP_LOGE(TAG, "Firmware hash mismatch");
        return FIRMWARE_STATUS_HASH_MISMATCH;
    }

    ESP_LOGI(TAG, "Firmware verification succeeded");
    return FIRMWARE_STATUS_OK;
}

esp_err_t firmware_verify_generate_header(const uint8_t *firmware_data, size_t firmware_size, 
                                          firmware_verify_header_t *header)
{
    if (!firmware_data || !header || firmware_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(header, 0, sizeof(firmware_verify_header_t));
    memcpy(header->magic, FIRMWARE_VERIFY_MAGIC, FIRMWARE_VERIFY_MAGIC_SIZE);
    header->version = FIRMWARE_VERIFY_VERSION;
    header->firmware_size = firmware_size;

    esp_sha(SHA2_256, firmware_data, firmware_size, header->sha256);

    ESP_LOGI(TAG, "Firmware verification header generated");
    return ESP_OK;
}

const char *firmware_verify_status_to_string(firmware_verify_status_t status)
{
    switch (status) {
        case FIRMWARE_STATUS_OK: return "OK";
        case FIRMWARE_STATUS_INVALID_HEADER: return "Invalid Header";
        case FIRMWARE_STATUS_HASH_MISMATCH: return "Hash Mismatch";
        case FIRMWARE_STATUS_TOO_LARGE: return "Too Large";
        case FIRMWARE_STATUS_CORRUPTED: return "Corrupted";
        default: return "Unknown";
    }
}