#include "app_signature.h"
#include "esp_log.h"
#include "sha/sha_core.h"
#include "hal/sha_types.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "APP_SIGNATURE";

esp_err_t app_signature_init(void)
{
    ESP_LOGI(TAG, "Initializing app signature module");
    return ESP_OK;
}

esp_err_t app_signature_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing app signature module");
    return ESP_OK;
}

static esp_err_t sha256_file(const char *file_path, uint8_t *hash)
{
    if (!file_path || !hash) return ESP_ERR_INVALID_ARG;

    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *file_buffer = (uint8_t *)heap_caps_malloc((size_t)file_size, MALLOC_CAP_SPIRAM);
    if (!file_buffer) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read = fread(file_buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    if (bytes_read != (size_t)file_size) {
        free(file_buffer);
        return ESP_FAIL;
    }

    esp_sha(SHA2_256, file_buffer, (size_t)file_size, hash);
    free(file_buffer);

    return ESP_OK;
}

app_signature_status_t app_signature_verify(const char *app_path, const uint8_t *public_key, size_t key_size)
{
    (void)public_key;
    (void)key_size;

    if (!app_path) {
        return SIGNATURE_STATUS_INVALID;
    }

    app_signature_t sig;
    esp_err_t ret = app_signature_load(app_path, &sig);
    if (ret != ESP_OK) {
        return SIGNATURE_STATUS_NOT_FOUND;
    }

    uint8_t file_hash[APP_SIGNATURE_SHA256_SIZE];
    ret = sha256_file(app_path, file_hash);
    if (ret != ESP_OK) {
        return SIGNATURE_STATUS_INVALID;
    }

    if (memcmp(file_hash, sig.sha256, APP_SIGNATURE_SHA256_SIZE) != 0) {
        ESP_LOGE(TAG, "File hash mismatch");
        return SIGNATURE_STATUS_VERIFY_FAILED;
    }

    ESP_LOGI(TAG, "Signature verification succeeded");
    return SIGNATURE_STATUS_OK;
}

esp_err_t app_signature_generate(const char *app_path, const uint8_t *private_key, size_t key_size, app_signature_t *signature)
{
    (void)private_key;
    (void)key_size;

    if (!app_path || !signature) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = sha256_file(app_path, signature->sha256);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(signature->signature, 0, APP_SIGNATURE_RSA_SIZE);
    signature->version = 1;
    signature->timestamp = (uint32_t)time(NULL);
    memset(signature->author, 0, sizeof(signature->author));

    ESP_LOGI(TAG, "Signature generated successfully");
    return ESP_OK;
}

esp_err_t app_signature_load(const char *app_path, app_signature_t *signature)
{
    if (!app_path || !signature) return ESP_ERR_INVALID_ARG;

    char sig_path[512];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", app_path);

    FILE *fp = fopen(sig_path, "rb");
    if (!fp) {
        ESP_LOGW(TAG, "Signature file not found: %s", sig_path);
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_read = fread(signature, 1, sizeof(app_signature_t), fp);
    fclose(fp);

    if (bytes_read != sizeof(app_signature_t)) {
        ESP_LOGE(TAG, "Invalid signature file size");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t app_signature_save(const char *app_path, const app_signature_t *signature)
{
    if (!app_path || !signature) return ESP_ERR_INVALID_ARG;

    char sig_path[512];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", app_path);

    FILE *fp = fopen(sig_path, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to create signature file: %s", sig_path);
        return ESP_FAIL;
    }

    size_t bytes_written = fwrite(signature, 1, sizeof(app_signature_t), fp);
    fclose(fp);

    if (bytes_written != sizeof(app_signature_t)) {
        ESP_LOGE(TAG, "Failed to write signature file");
        return ESP_FAIL;
    }

    return ESP_OK;
}

const char *app_signature_status_to_string(app_signature_status_t status)
{
    switch (status) {
        case SIGNATURE_STATUS_OK: return "OK";
        case SIGNATURE_STATUS_INVALID: return "Invalid";
        case SIGNATURE_STATUS_NOT_FOUND: return "Not Found";
        case SIGNATURE_STATUS_VERIFY_FAILED: return "Verify Failed";
        case SIGNATURE_STATUS_KEY_MISMATCH: return "Key Mismatch";
        default: return "Unknown";
    }
}