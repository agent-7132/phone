#include "ota_manager.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

static const char *TAG = "OTA_MANAGER";

static ota_state_t s_ota_state = OTA_STATE_IDLE;
static int s_progress = 0;
static ota_progress_cb_t s_progress_cb = NULL;
static ota_complete_cb_t s_complete_cb = NULL;
static SemaphoreHandle_t s_ota_mutex = NULL;

#define BUFFER_SIZE 1024
#define OTA_TASK_STACK_SIZE 8192

static void update_progress(int progress)
{
    s_progress = progress;
    if (s_progress_cb) {
        s_progress_cb(progress, s_ota_state);
    }
}

static void set_state(ota_state_t state)
{
    s_ota_state = state;
    if (s_progress_cb) {
        s_progress_cb(s_progress, state);
    }
}

static void ota_task(void *arg)
{
    const char *url = (const char *)arg;
    ESP_LOGI(TAG, "Starting OTA update from: %s", url);
    
    set_state(OTA_STATE_DOWNLOADING);
    update_progress(0);
    
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Downloading firmware: %d bytes", content_length);
    
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "Failed to get update partition");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Writing to partition: %s at offset: 0x%x", 
             update_partition->label, update_partition->address);
    
    esp_ota_handle_t update_handle = 0;
    ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    char buffer[BUFFER_SIZE];
    int total_read = 0;
    int read_len = 0;
    
    while (1) {
        read_len = esp_http_client_read(client, buffer, BUFFER_SIZE);
        if (read_len <= 0) break;
        
        ret = esp_ota_write(update_handle, (const void *)buffer, read_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            set_state(OTA_STATE_FAILED);
            if (s_complete_cb) s_complete_cb(false);
            heap_caps_free((void *)url);
            vTaskDelete(NULL);
            return;
        }
        
        total_read += read_len;
        int progress = (total_read * 100) / content_length;
        update_progress(progress);
        
        ESP_LOGD(TAG, "Downloaded %d/%d bytes (%d%%)", total_read, content_length, progress);
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (total_read != content_length) {
        ESP_LOGE(TAG, "Download incomplete: expected %d, got %d", content_length, total_read);
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    set_state(OTA_STATE_VERIFYING);
    ESP_LOGI(TAG, "Verifying firmware...");
    
    ret = esp_ota_end(update_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    set_state(OTA_STATE_UPDATING);
    ESP_LOGI(TAG, "Setting boot partition...");
    
    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        set_state(OTA_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false);
        heap_caps_free((void *)url);
        vTaskDelete(NULL);
        return;
    }
    
    set_state(OTA_STATE_COMPLETED);
    update_progress(100);
    ESP_LOGI(TAG, "OTA update completed successfully");
    
    if (s_complete_cb) s_complete_cb(true);
    
    heap_caps_free((void *)url);
    vTaskDelete(NULL);
}

esp_err_t ota_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing OTA manager...");
    
    s_ota_mutex = xSemaphoreCreateMutex();
    if (!s_ota_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "OTA manager initialized");
    return ESP_OK;
}

esp_err_t ota_manager_start_update(const char *firmware_url)
{
    if (!firmware_url) {
        ESP_LOGE(TAG, "Firmware URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(s_ota_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_FAIL;
    }
    
    if (s_ota_state != OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress: %d", s_ota_state);
        xSemaphoreGive(s_ota_mutex);
        return ESP_FAIL;
    }
    
    xSemaphoreGive(s_ota_mutex);
    
    char *url_copy = heap_caps_malloc(strlen(firmware_url) + 1, MALLOC_CAP_SPIRAM);
    if (!url_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_ERR_NO_MEM;
    }
    strcpy(url_copy, firmware_url);
    
    BaseType_t ret = xTaskCreate(ota_task, "ota_task", OTA_TASK_STACK_SIZE, 
                                 url_copy, 8, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        heap_caps_free(url_copy);
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

ota_state_t ota_manager_get_state(void)
{
    return s_ota_state;
}

int ota_manager_get_progress(void)
{
    return s_progress;
}

void ota_manager_register_progress_cb(ota_progress_cb_t cb)
{
    s_progress_cb = cb;
}

void ota_manager_register_complete_cb(ota_complete_cb_t cb)
{
    s_complete_cb = cb;
}

void ota_manager_reboot(void)
{
    ESP_LOGI(TAG, "Rebooting to apply new firmware...");
    esp_restart();
}