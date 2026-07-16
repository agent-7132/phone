#include "secure_storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "aes/esp_aes.h"
#include "tinycrypt/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_random.h"

static const char *TAG = "SECURE_STORAGE";
static const char *NVS_NAMESPACE = "secure";

#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16
#define MAX_VALUE_SIZE 512

static SemaphoreHandle_t s_mutex = NULL;
static unsigned char s_encryption_key[AES_KEY_SIZE];

static void derive_key_from_password(const char *password, unsigned char *key)
{
    unsigned char salt[8] = {0x53, 0x45, 0x43, 0x55, 0x52, 0x45, 0x5F, 0x4B};
    
    struct tc_sha256_state_struct ctx;
    tc_sha256_init(&ctx);
    tc_sha256_update(&ctx, (const uint8_t *)password, strlen(password));
    tc_sha256_update(&ctx, salt, sizeof(salt));
    tc_sha256_final(key, &ctx);
}

static esp_err_t encrypt_data(const unsigned char *plaintext, size_t plaintext_len,
                              unsigned char *ciphertext, size_t *ciphertext_len,
                              unsigned char *iv)
{
    if (plaintext_len > MAX_VALUE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < AES_IV_SIZE; i++) {
        iv[i] = (unsigned char)(esp_random() & 0xFF);
    }
    
    size_t padding_len = AES_IV_SIZE - (plaintext_len % AES_IV_SIZE);
    *ciphertext_len = AES_IV_SIZE + plaintext_len + padding_len;
    
    unsigned char padded_data[MAX_VALUE_SIZE + AES_IV_SIZE];
    memcpy(padded_data, plaintext, plaintext_len);
    for (size_t i = plaintext_len; i < plaintext_len + padding_len; i++) {
        padded_data[i] = (unsigned char)padding_len;
    }
    
    esp_aes_context aes_ctx;
    esp_aes_init(&aes_ctx);
    esp_aes_setkey(&aes_ctx, s_encryption_key, AES_KEY_SIZE * 8);
    
    memcpy(ciphertext, iv, AES_IV_SIZE);
    
    unsigned char prev_block[AES_IV_SIZE];
    memcpy(prev_block, iv, AES_IV_SIZE);
    
    for (size_t i = 0; i < plaintext_len + padding_len; i += AES_IV_SIZE) {
        unsigned char block[AES_IV_SIZE];
        memcpy(block, padded_data + i, AES_IV_SIZE);
        
        for (size_t j = 0; j < AES_IV_SIZE; j++) {
            block[j] ^= prev_block[j];
        }
        
        esp_aes_crypt_ecb(&aes_ctx, 1, block, ciphertext + AES_IV_SIZE + i);
        memcpy(prev_block, ciphertext + AES_IV_SIZE + i, AES_IV_SIZE);
    }
    
    esp_aes_free(&aes_ctx);
    return ESP_OK;
}

static esp_err_t decrypt_data(const unsigned char *ciphertext, size_t ciphertext_len,
                              unsigned char *plaintext, size_t *plaintext_len)
{
    if (ciphertext_len < AES_IV_SIZE * 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    unsigned char iv[AES_IV_SIZE];
    memcpy(iv, ciphertext, AES_IV_SIZE);
    
    esp_aes_context aes_ctx;
    esp_aes_init(&aes_ctx);
    esp_aes_setkey(&aes_ctx, s_encryption_key, AES_KEY_SIZE * 8);
    
    size_t data_len = ciphertext_len - AES_IV_SIZE;
    unsigned char decrypted_data[MAX_VALUE_SIZE + AES_IV_SIZE];
    
    unsigned char prev_block[AES_IV_SIZE];
    memcpy(prev_block, iv, AES_IV_SIZE);
    
    for (size_t i = 0; i < data_len; i += AES_IV_SIZE) {
        unsigned char block[AES_IV_SIZE];
        memcpy(block, ciphertext + AES_IV_SIZE + i, AES_IV_SIZE);
        
        unsigned char plain_block[AES_IV_SIZE];
        esp_aes_crypt_ecb(&aes_ctx, 0, block, plain_block);
        
        for (size_t j = 0; j < AES_IV_SIZE; j++) {
            plain_block[j] ^= prev_block[j];
        }
        
        memcpy(decrypted_data + i, plain_block, AES_IV_SIZE);
        memcpy(prev_block, block, AES_IV_SIZE);
    }
    
    esp_aes_free(&aes_ctx);
    
    unsigned char padding_len = decrypted_data[data_len - 1];
    if (padding_len > AES_IV_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *plaintext_len = data_len - padding_len;
    memcpy(plaintext, decrypted_data, *plaintext_len);
    plaintext[*plaintext_len] = '\0';
    
    return ESP_OK;
}

esp_err_t secure_storage_init(void)
{
    ESP_LOGI(TAG, "Initializing secure storage...");
    
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    derive_key_from_password("esp32_phone_secure_key", s_encryption_key);
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash");
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Secure storage initialized");
    return ESP_OK;
}

esp_err_t secure_storage_set(const char *key, const char *value)
{
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    unsigned char iv[AES_IV_SIZE];
    unsigned char ciphertext[MAX_VALUE_SIZE + AES_IV_SIZE * 2];
    size_t ciphertext_len;
    
    ret = encrypt_data((const unsigned char *)value, strlen(value), ciphertext, &ciphertext_len, iv);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encryption failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_set_blob(handle, key, ciphertext, ciphertext_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set blob: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    
    ESP_LOGD(TAG, "Secure storage set: %s", key);
    return ESP_OK;
}

esp_err_t secure_storage_get(const char *key, char *value, size_t max_len)
{
    if (!key || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    size_t ciphertext_len = 0;
    ret = nvs_get_blob(handle, key, NULL, &ciphertext_len);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(handle);
            xSemaphoreGive(s_mutex);
            return ESP_ERR_NVS_NOT_FOUND;
        }
        ESP_LOGE(TAG, "Failed to get blob size: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    if (ciphertext_len > MAX_VALUE_SIZE + AES_IV_SIZE * 2) {
        ESP_LOGE(TAG, "Blob too large");
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    
    unsigned char ciphertext[MAX_VALUE_SIZE + AES_IV_SIZE * 2];
    ret = nvs_get_blob(handle, key, ciphertext, &ciphertext_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get blob: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    nvs_close(handle);
    
    unsigned char plaintext[MAX_VALUE_SIZE + 1];
    size_t plaintext_len;
    
    ret = decrypt_data(ciphertext, ciphertext_len, plaintext, &plaintext_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Decryption failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    if (plaintext_len >= max_len) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    
    memcpy(value, plaintext, plaintext_len + 1);
    xSemaphoreGive(s_mutex);
    
    ESP_LOGD(TAG, "Secure storage get: %s", key);
    return ESP_OK;
}

esp_err_t secure_storage_delete(const char *key)
{
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_erase_key(handle, key);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            nvs_close(handle);
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Failed to erase key: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    
    ESP_LOGD(TAG, "Secure storage delete: %s", key);
    return ESP_OK;
}

bool secure_storage_exists(const char *key)
{
    if (!key) {
        return false;
    }
    
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return false;
    }
    
    size_t len = 0;
    ret = nvs_get_blob(handle, key, NULL, &len);
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    
    return (ret == ESP_OK);
}

esp_err_t secure_storage_clear_all(void)
{
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_erase_all(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase all: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    ret = nvs_commit(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(handle);
        xSemaphoreGive(s_mutex);
        return ret;
    }
    
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    
    ESP_LOGI(TAG, "Secure storage cleared");
    return ESP_OK;
}
