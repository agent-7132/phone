#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_BRIGHTNESS_MIN    10
#define SETTINGS_BRIGHTNESS_MAX    100
#define SETTINGS_BRIGHTNESS_DEFAULT 70

#define SETTINGS_VOLUME_MIN        0
#define SETTINGS_VOLUME_MAX        100
#define SETTINGS_VOLUME_DEFAULT    50

/**
 * @brief Initialize settings manager
 *
 * Loads saved brightness/volume from NVS, initializes audio codec,
 * and applies the saved values to hardware.
 *
 * @return ESP_OK on success
 */
esp_err_t settings_manager_init(void);

/**
 * @brief Set display brightness
 *
 * @param brightness_percent Brightness in percent (10-100)
 * @return ESP_OK on success
 */
esp_err_t settings_manager_set_brightness(int brightness_percent);

/**
 * @brief Get current display brightness
 *
 * @return Brightness in percent
 */
int settings_manager_get_brightness(void);

/**
 * @brief Set audio volume
 *
 * @param volume_percent Volume in percent (0-100)
 * @return ESP_OK on success
 */
esp_err_t settings_manager_set_volume(int volume_percent);

/**
 * @brief Get current audio volume
 *
 * @return Volume in percent
 */
int settings_manager_get_volume(void);

/**
 * @brief Check if audio codec is initialized
 *
 * @return true if initialized, false otherwise
 */
bool settings_manager_audio_ready(void);

#ifdef __cplusplus
}
#endif
