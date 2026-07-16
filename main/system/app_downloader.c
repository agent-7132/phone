#include "app_downloader.h"
#include "app_installer.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include <sys/stat.h>
#include <stdio.h>

static const char *TAG = "APP_DOWNLOADER";

#define DOWNLOAD_TASK_STACK_SIZE 8192
#define DOWNLOAD_BUFFER_SIZE 1024
#define TEMP_DOWNLOAD_DIR "/spiffs/temp"

static download_state_t s_state = DOWNLOAD_STATE_IDLE;
static int s_progress = 0;
static download_progress_cb_t s_progress_cb = NULL;
static download_complete_cb_t s_complete_cb = NULL;
static SemaphoreHandle_t s_mutex = NULL;

static void update_progress(int progress)
{
    s_progress = progress;
    if (s_progress_cb) {
        s_progress_cb(progress, s_state);
    }
}

static void set_state(download_state_t state)
{
    s_state = state;
    if (s_progress_cb) {
        s_progress_cb(s_progress, state);
    }
}

static void download_task(void *arg)
{
    char *url = (char *)arg;
    char app_name[64];
    char temp_file[128];

    const char *last_slash = strrchr(url, '/');
    const char *last_dot = strrchr(url, '.');
    if (last_slash && last_dot && last_dot > last_slash) {
        int len = last_dot - last_slash - 1;
        if (len > sizeof(app_name) - 1) len = sizeof(app_name) - 1;
        strncpy(app_name, last_slash + 1, len);
        app_name[len] = '\0';
    } else {
        strcpy(app_name, "downloaded_app");
    }

    snprintf(temp_file, sizeof(temp_file), "%s/%s.epp", TEMP_DOWNLOAD_DIR, app_name);

    ESP_LOGI(TAG, "Downloading app: %s from %s", app_name, url);

    set_state(DOWNLOAD_STATE_DOWNLOADING);
    update_progress(0);

    mkdir(TEMP_DOWNLOAD_DIR, 0755);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
        vTaskDelete(NULL);
        heap_caps_free(url);
        return;
    }

    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
        vTaskDelete(NULL);
        heap_caps_free(url);
        return;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
        vTaskDelete(NULL);
        heap_caps_free(url);
        return;
    }

    ESP_LOGI(TAG, "Downloading file: %d bytes", content_length);

    FILE *f = fopen(temp_file, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create temp file: %s", temp_file);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
        vTaskDelete(NULL);
        heap_caps_free(url);
        return;
    }

    char buffer[DOWNLOAD_BUFFER_SIZE];
    int total_read = 0;
    int read_len;

    while (1) {
        read_len = esp_http_client_read(client, buffer, DOWNLOAD_BUFFER_SIZE);
        if (read_len <= 0) break;

        fwrite(buffer, 1, read_len, f);
        total_read += read_len;

        int progress = (total_read * 100) / content_length;
        update_progress(progress);

        ESP_LOGD(TAG, "Downloaded %d/%d bytes (%d%%)", total_read, content_length, progress);
    }

    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read != content_length) {
        ESP_LOGE(TAG, "Download incomplete: expected %d, got %d", content_length, total_read);
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
        vTaskDelete(NULL);
        heap_caps_free(url);
        return;
    }

    ESP_LOGI(TAG, "Download complete, installing...");
    set_state(DOWNLOAD_STATE_INSTALLING);

    install_result_t install_ret = app_installer_install(temp_file);
    if (install_ret == INSTALL_OK) {
        ESP_LOGI(TAG, "App installed successfully: %s", app_name);
        set_state(DOWNLOAD_STATE_COMPLETED);
        update_progress(100);
        if (s_complete_cb) s_complete_cb(true, app_name);
    } else {
        ESP_LOGE(TAG, "Install failed: %d", install_ret);
        set_state(DOWNLOAD_STATE_FAILED);
        if (s_complete_cb) s_complete_cb(false, app_name);
    }

    vTaskDelete(NULL);
    free(url);
}

void app_downloader_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    ESP_LOGI(TAG, "App downloader initialized");
}

esp_err_t app_downloader_start(const char *url, const char *app_name,
                               download_progress_cb_t progress_cb,
                               download_complete_cb_t complete_cb)
{
    if (!url) {
        ESP_LOGE(TAG, "URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (s_state != DOWNLOAD_STATE_IDLE) {
        ESP_LOGW(TAG, "Download already in progress");
        xSemaphoreGive(s_mutex);
        return ESP_FAIL;
    }

    s_progress_cb = progress_cb;
    s_complete_cb = complete_cb;

    char *url_copy = heap_caps_malloc(strlen(url) + 1, MALLOC_CAP_SPIRAM);
    if (!url_copy) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }
    strcpy(url_copy, url);

    BaseType_t ret = xTaskCreate(download_task, "app_download", DOWNLOAD_TASK_STACK_SIZE,
                                 url_copy, 4, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create download task");
        heap_caps_free(url_copy);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

download_state_t app_downloader_get_state(void)
{
    return s_state;
}

int app_downloader_get_progress(void)
{
    return s_progress;
}

void app_downloader_cancel(void)
{
    set_state(DOWNLOAD_STATE_FAILED);
}