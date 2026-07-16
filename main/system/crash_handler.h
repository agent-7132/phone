#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRASH_LOG_SIZE 4096
#define CRASH_LOG_FILE "/spiffs/crash.log"

typedef enum {
    CRASH_TYPE_UNKNOWN,
    CRASH_TYPE_ASSERT,
    CRASH_TYPE_WATCHDOG,
    CRASH_TYPE_EXCEPTION,
    CRASH_TYPE_STACK_OVERFLOW
} crash_type_t;

typedef struct {
    crash_type_t type;
    uint32_t timestamp;
    uint32_t pc;
    uint32_t sp;
    uint32_t ps;
    char message[256];
    char stack_trace[CRASH_LOG_SIZE];
} crash_info_t;

esp_err_t crash_handler_init(void);

void crash_handler_register_callback(void (*cb)(crash_info_t *info));

void crash_handler_save_log(crash_type_t type, const char *message);

void crash_handler_save_log_with_context(crash_type_t type, const char *message, 
                                         uint32_t pc, uint32_t sp, uint32_t ps);

bool crash_handler_has_crash_log(void);

esp_err_t crash_handler_read_log(crash_info_t *info);

esp_err_t crash_handler_clear_log(void);

void crash_handler_reboot(void);

void crash_handler_watchdog_enable(uint32_t timeout_ms);

void crash_handler_watchdog_disable(void);

void crash_handler_watchdog_feed(void);

#ifdef __cplusplus
}
#endif