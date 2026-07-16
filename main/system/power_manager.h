#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POWER_STATE_DISCHARGING,
    POWER_STATE_CHARGING,
    POWER_STATE_FULL,
    POWER_STATE_AC_POWERED,
    POWER_STATE_UNKNOWN
} power_state_t;

typedef enum {
    POWER_MODE_NORMAL,
    POWER_MODE_LOW_POWER,
    POWER_MODE_SLEEP
} power_mode_t;

typedef struct {
    int battery_level;
    power_state_t state;
    float voltage;
    bool ac_present;
} power_info_t;

typedef void (*power_event_cb_t)(power_info_t *info);

esp_err_t power_manager_init(void);
void power_manager_get_info(power_info_t *info);
power_mode_t power_manager_get_mode(void);
esp_err_t power_manager_set_mode(power_mode_t mode);
void power_manager_register_event_cb(power_event_cb_t cb);
int power_manager_get_battery_level(void);
bool power_manager_is_charging(void);

void power_manager_reset_activity_timer(void);
void power_manager_set_auto_sleep_timeout(int seconds);
int power_manager_get_auto_sleep_timeout(void);
void power_manager_set_auto_sleep_enabled(bool enabled);
bool power_manager_is_auto_sleep_enabled(void);

#ifdef __cplusplus
}
#endif