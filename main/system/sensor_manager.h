#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SENSORS 5
#define MAX_SENSOR_NAME 32

typedef enum {
    SENSOR_TYPE_UNKNOWN,
    SENSOR_TYPE_TEMPERATURE,
    SENSOR_TYPE_HUMIDITY,
    SENSOR_TYPE_ACCELEROMETER,
    SENSOR_TYPE_GYROSCOPE,
    SENSOR_TYPE_PRESSURE,
    SENSOR_TYPE_LIGHT
} sensor_type_t;

typedef struct {
    float temperature;
    float humidity;
} sensor_env_data_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} sensor_accel_data_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} sensor_gyro_data_t;

typedef union {
    sensor_env_data_t env;
    sensor_accel_data_t accel;
    sensor_gyro_data_t gyro;
    float pressure;
    float light;
} sensor_data_t;

typedef struct sensor sensor_t;

struct sensor {
    char name[MAX_SENSOR_NAME];
    sensor_type_t type;
    bool enabled;
    bool (*init)(sensor_t *sensor);
    bool (*read)(sensor_t *sensor, sensor_data_t *data);
    void (*deinit)(sensor_t *sensor);
    void *priv_data;
};

typedef void (*sensor_data_cb_t)(sensor_type_t type, sensor_data_t *data);

esp_err_t sensor_manager_init(void);

esp_err_t sensor_manager_register(sensor_t *sensor);

esp_err_t sensor_manager_unregister(const char *name);

sensor_t *sensor_manager_find(const char *name);

int sensor_manager_get_sensor_count(void);

sensor_t *sensor_manager_get_sensor(int index);

esp_err_t sensor_manager_set_enabled(const char *name, bool enabled);

esp_err_t sensor_manager_read(const char *name, sensor_data_t *data);

esp_err_t sensor_manager_read_all(void);

void sensor_manager_register_data_cb(sensor_data_cb_t cb);

bool sensor_sht3x_init(sensor_t *sensor);
bool sensor_sht3x_read(sensor_t *sensor, sensor_data_t *data);
void sensor_sht3x_deinit(sensor_t *sensor);

bool sensor_lis3dh_init(sensor_t *sensor);
bool sensor_lis3dh_read(sensor_t *sensor, sensor_data_t *data);
void sensor_lis3dh_deinit(sensor_t *sensor);

#ifdef __cplusplus
}
#endif