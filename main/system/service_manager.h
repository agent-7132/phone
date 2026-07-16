#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_MANAGER_MAX_SERVICES 10
#define SERVICE_MANAGER_MAX_DEPENDENCIES 5

typedef enum {
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_PAUSED,
    SERVICE_STATE_CRASHED,
    SERVICE_STATE_STARTING
} service_state_t;

typedef enum {
    SERVICE_PRIORITY_LOW = 1,
    SERVICE_PRIORITY_MEDIUM = 5,
    SERVICE_PRIORITY_HIGH = 10,
    SERVICE_PRIORITY_CRITICAL = 15
} service_priority_t;

typedef struct {
    char name[32];
    esp_err_t (*start)(void *arg);
    void (*stop)(void *arg);
    void (*pause)(void *arg);
    void (*resume)(void *arg);
    bool (*health_check)(void *arg);
    void *arg;
    service_state_t state;
    service_priority_t priority;
    TaskHandle_t task_handle;
    size_t stack_size;
    
    bool auto_restart;
    int restart_count;
    int max_restarts;
    uint32_t restart_delay_ms;
    
    uint32_t max_memory_bytes;
    uint32_t cpu_usage_percent;
    uint32_t health_check_interval_ms;
    
    char dependencies[SERVICE_MANAGER_MAX_DEPENDENCIES][32];
    int dependency_count;
    
    uint64_t start_time;
    uint64_t total_run_time;
    uint64_t last_health_check;
    
    bool resource_limited;
} service_info_t;

typedef void (*service_event_cb_t)(const char *name, service_state_t state, void *arg);

esp_err_t service_manager_init(void);

esp_err_t service_manager_register(const char *name, 
                                   esp_err_t (*start)(void *arg),
                                   void (*stop)(void *arg),
                                   void (*pause)(void *arg),
                                   void (*resume)(void *arg),
                                   bool (*health_check)(void *arg),
                                   service_priority_t priority, 
                                   size_t stack_size,
                                   void *arg);

esp_err_t service_manager_unregister(const char *name);

esp_err_t service_manager_start(const char *name);

esp_err_t service_manager_stop(const char *name);

esp_err_t service_manager_pause(const char *name);

esp_err_t service_manager_resume(const char *name);

service_state_t service_manager_get_state(const char *name);

service_info_t *service_manager_get_service(const char *name);

service_info_t *service_manager_get_service_by_index(int index);

int service_manager_get_service_count(void);

void service_manager_dump_services(void);

esp_err_t service_manager_add_dependency(const char *name, const char *dep_name);

esp_err_t service_manager_set_auto_restart(const char *name, bool enable, int max_restarts, uint32_t delay_ms);

esp_err_t service_manager_set_resource_limits(const char *name, uint32_t max_memory_bytes, uint32_t cpu_usage_percent);

esp_err_t service_manager_set_health_check(const char *name, bool (*health_check)(void *arg), uint32_t interval_ms);

void service_manager_pause_low_priority(int min_priority);

void service_manager_resume_all(void);

void service_manager_register_event_callback(service_event_cb_t cb, void *arg);

void service_manager_run_health_checks(void);

#ifdef __cplusplus
}
#endif