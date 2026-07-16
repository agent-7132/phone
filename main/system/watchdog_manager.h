#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WATCHDOG_DEFAULT_TIMEOUT_MS 30000
#define WATCHDOG_MIN_TIMEOUT_MS 1000
#define WATCHDOG_MAX_TIMEOUT_MS 300000

typedef enum {
    WATCHDOG_MODE_IDLE,
    WATCHDOG_MODE_ACTIVE,
    WATCHDOG_MODE_PAUSED
} watchdog_mode_t;

typedef void (*watchdog_timeout_cb_t)(void);

esp_err_t watchdog_manager_init(void);

esp_err_t watchdog_manager_start(uint32_t timeout_ms);

void watchdog_manager_stop(void);

void watchdog_manager_feed(void);

void watchdog_manager_pause(void);

void watchdog_manager_resume(void);

watchdog_mode_t watchdog_manager_get_mode(void);

void watchdog_manager_register_timeout_callback(watchdog_timeout_cb_t cb);

uint32_t watchdog_manager_get_timeout_ms(void);

bool watchdog_manager_is_running(void);

#ifdef __cplusplus
}
#endif