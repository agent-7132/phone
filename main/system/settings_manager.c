#include "settings_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "bsp/display.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "SETTINGS_MGR";

#define NVS_NAMESPACE "settings"

static int s_brightness = SETTINGS_BRIGHTNESS_DEFAULT;
static int s_volume = SETTINGS_VOLUME_DEFAULT;
static bool s_audio_ready = false;
static esp_codec_dev_handle_t s_speaker = NULL;

static void load_settings(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed, using defaults: %s", esp_err_to_name(ret));
        return;
    }

    int8_t brightness = SETTINGS_BRIGHTNESS_DEFAULT;
    int8_t volume = SETTINGS_VOLUME_DEFAULT;
    nvs_get_i8(handle, "brightness", &brightness);
    nvs_get_i8(handle, "volume", &volume);
    nvs_close(handle);

    if (brightness >= SETTINGS_BRIGHTNESS_MIN && brightness <= SETTINGS_BRIGHTNESS_MAX) {
        s_brightness = brightness;
    }
    if (volume >= SETTINGS_VOLUME_MIN && volume <= SETTINGS_VOLUME_MAX) {
        s_volume = volume;
    }

    ESP_LOGI(TAG, "Loaded settings: brightness=%d, volume=%d", s_brightness, s_volume);
}

static void save_brightness(int brightness)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    int8_t val = (int8_t)brightness;
    nvs_set_i8(handle, "brightness", val);
    nvs_commit(handle);
    nvs_close(handle);
}

static void save_volume(int volume)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    int8_t val = (int8_t)volume;
    nvs_set_i8(handle, "volume", val);
    nvs_commit(handle);
    nvs_close(handle);
}

static void init_audio_codec(void)
{
    esp_err_t ret = bsp_audio_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio init failed: %s", esp_err_to_name(ret));
        return;
    }

    s_speaker = bsp_audio_codec_speaker_init();
    if (s_speaker) {
        s_audio_ready = true;
        esp_codec_dev_set_out_vol(s_speaker, s_volume);
        ESP_LOGI(TAG, "Audio codec initialized, volume=%d", s_volume);
    } else {
        ESP_LOGW(TAG, "Speaker codec init failed");
    }
}

esp_err_t settings_manager_init(void)
{
    load_settings();

    esp_err_t ret = bsp_display_brightness_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Brightness init failed: %s", esp_err_to_name(ret));
    } else {
        bsp_display_brightness_set(s_brightness);
        ESP_LOGI(TAG, "Brightness initialized to %d%%", s_brightness);
    }

    init_audio_codec();

    ESP_LOGI(TAG, "Settings manager initialized");
    return ESP_OK;
}

esp_err_t settings_manager_set_brightness(int brightness_percent)
{
    if (brightness_percent < SETTINGS_BRIGHTNESS_MIN) {
        brightness_percent = SETTINGS_BRIGHTNESS_MIN;
    }
    if (brightness_percent > SETTINGS_BRIGHTNESS_MAX) {
        brightness_percent = SETTINGS_BRIGHTNESS_MAX;
    }

    s_brightness = brightness_percent;
    esp_err_t ret = bsp_display_brightness_set(brightness_percent);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Set brightness failed: %s", esp_err_to_name(ret));
        return ret;
    }

    save_brightness(brightness_percent);
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness_percent);
    return ESP_OK;
}

int settings_manager_get_brightness(void)
{
    return s_brightness;
}

esp_err_t settings_manager_set_volume(int volume_percent)
{
    if (volume_percent < SETTINGS_VOLUME_MIN) {
        volume_percent = SETTINGS_VOLUME_MIN;
    }
    if (volume_percent > SETTINGS_VOLUME_MAX) {
        volume_percent = SETTINGS_VOLUME_MAX;
    }

    s_volume = volume_percent;

    if (s_audio_ready && s_speaker) {
        esp_err_t ret = esp_codec_dev_set_out_vol(s_speaker, volume_percent);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Set volume failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    save_volume(volume_percent);
    ESP_LOGI(TAG, "Volume set to %d%%", volume_percent);
    return ESP_OK;
}

int settings_manager_get_volume(void)
{
    return s_volume;
}

bool settings_manager_audio_ready(void)
{
    return s_audio_ready;
}
