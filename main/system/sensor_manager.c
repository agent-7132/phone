#include "sensor_manager.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "bsp/esp32_p4_platform.h"

static const char *TAG = "SENSOR_MANAGER";

static sensor_t sensors[MAX_SENSORS];
static int sensor_count = 0;
static sensor_data_cb_t data_cb = NULL;

#define SHT3X_ADDR 0x44
#define LIS3DH_ADDR 0x18

static i2c_master_dev_handle_t sht3x_dev = NULL;
static i2c_master_dev_handle_t lis3dh_dev = NULL;

static uint16_t sht3x_crc(uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x80) ? 0x31 : 0);
        }
    }
    return crc;
}

bool sensor_sht3x_init(sensor_t *sensor)
{
    (void)sensor;
    
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not available");
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .device_address = SHT3X_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &sht3x_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SHT3x device: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "SHT3x sensor initialized");
    return true;
}

bool sensor_sht3x_read(sensor_t *sensor, sensor_data_t *data)
{
    (void)sensor;

    if (!sht3x_dev) {
        ESP_LOGE(TAG, "SHT3x device not initialized");
        return false;
    }

    uint8_t cmd[2] = {0x2C, 0x06};
    esp_err_t ret = i2c_master_transmit(sht3x_dev, cmd, sizeof(cmd), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send SHT3x command: %s", esp_err_to_name(ret));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t raw_data[6];
    ret = i2c_master_receive(sht3x_dev, raw_data, sizeof(raw_data), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SHT3x data: %s", esp_err_to_name(ret));
        return false;
    }

    uint8_t crc1 = raw_data[2];
    uint8_t crc2 = raw_data[5];
    uint16_t calc_crc1 = sht3x_crc(raw_data, 2);
    uint16_t calc_crc2 = sht3x_crc(raw_data + 3, 2);

    if (crc1 != calc_crc1 || crc2 != calc_crc2) {
        ESP_LOGE(TAG, "SHT3x CRC error");
        return false;
    }

    uint16_t raw_temp = (raw_data[0] << 8) | raw_data[1];
    uint16_t raw_hum = (raw_data[3] << 8) | raw_data[4];

    data->env.temperature = -45.0 + 175.0 * ((float)raw_temp / 65535.0);
    data->env.humidity = 100.0 * ((float)raw_hum / 65535.0);

    ESP_LOGI(TAG, "SHT3x: Temperature=%.2f°C, Humidity=%.2f%%",
             data->env.temperature, data->env.humidity);

    return true;
}

void sensor_sht3x_deinit(sensor_t *sensor)
{
    (void)sensor;
    if (sht3x_dev) {
        i2c_master_bus_rm_device(sht3x_dev);
        sht3x_dev = NULL;
    }
    ESP_LOGI(TAG, "SHT3x sensor deinitialized");
}

bool sensor_lis3dh_init(sensor_t *sensor)
{
    (void)sensor;
    
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus not available");
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .device_address = LIS3DH_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &lis3dh_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add LIS3DH device: %s", esp_err_to_name(ret));
        return false;
    }

    uint8_t init_data[2] = {0x20, 0x47};
    ret = i2c_master_transmit(lis3dh_dev, init_data, sizeof(init_data), -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write LIS3DH CTRL_REG1: %s", esp_err_to_name(ret));
        i2c_master_bus_rm_device(lis3dh_dev);
        lis3dh_dev = NULL;
        return false;
    }

    ESP_LOGI(TAG, "LIS3DH sensor initialized");
    return true;
}

bool sensor_lis3dh_read(sensor_t *sensor, sensor_data_t *data)
{
    (void)sensor;

    if (!lis3dh_dev) {
        ESP_LOGE(TAG, "LIS3DH device not initialized");
        return false;
    }

    uint8_t cmd = 0xA8;
    uint8_t raw_data[6];
    esp_err_t ret = i2c_master_transmit_receive(lis3dh_dev, &cmd, 1, raw_data, 6, -1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LIS3DH data: %s", esp_err_to_name(ret));
        return false;
    }

    data->accel.x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    data->accel.y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    data->accel.z = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    ESP_LOGI(TAG, "LIS3DH: X=%d, Y=%d, Z=%d",
             data->accel.x, data->accel.y, data->accel.z);

    return true;
}

void sensor_lis3dh_deinit(sensor_t *sensor)
{
    (void)sensor;
    if (lis3dh_dev) {
        i2c_master_bus_rm_device(lis3dh_dev);
        lis3dh_dev = NULL;
    }
    ESP_LOGI(TAG, "LIS3DH sensor deinitialized");
}

static bool i2c_probe_device(uint8_t addr)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        return false;
    }
    
    return i2c_master_probe(bus, addr, 100) == ESP_OK;
}

esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing sensor manager...");

    sensor_count = 0;
    data_cb = NULL;

    bsp_i2c_init();

    if (i2c_probe_device(SHT3X_ADDR)) {
        ESP_LOGI(TAG, "SHT3x sensor detected at I2C address 0x%02X", SHT3X_ADDR);
        sensor_t sht3x_sensor = {
            .name = "sht3x",
            .type = SENSOR_TYPE_TEMPERATURE,
            .enabled = true,
            .init = sensor_sht3x_init,
            .read = sensor_sht3x_read,
            .deinit = sensor_sht3x_deinit,
            .priv_data = NULL
        };
        sensor_manager_register(&sht3x_sensor);
    } else {
        ESP_LOGW(TAG, "SHT3x sensor not detected at I2C address 0x%02X", SHT3X_ADDR);
    }

    if (i2c_probe_device(LIS3DH_ADDR)) {
        ESP_LOGI(TAG, "LIS3DH sensor detected at I2C address 0x%02X", LIS3DH_ADDR);
        sensor_t lis3dh_sensor = {
            .name = "lis3dh",
            .type = SENSOR_TYPE_ACCELEROMETER,
            .enabled = true,
            .init = sensor_lis3dh_init,
            .read = sensor_lis3dh_read,
            .deinit = sensor_lis3dh_deinit,
            .priv_data = NULL
        };
        sensor_manager_register(&lis3dh_sensor);
    } else {
        ESP_LOGW(TAG, "LIS3DH sensor not detected at I2C address 0x%02X", LIS3DH_ADDR);
    }

    ESP_LOGI(TAG, "Sensor manager initialized, %d sensors registered", sensor_count);
    return ESP_OK;
}

esp_err_t sensor_manager_register(sensor_t *sensor)
{
    if (sensor_count >= MAX_SENSORS) {
        ESP_LOGE(TAG, "Maximum sensors reached");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < sensor_count; i++) {
        if (strcmp(sensors[i].name, sensor->name) == 0) {
            ESP_LOGE(TAG, "Sensor '%s' already registered", sensor->name);
            return ESP_ERR_INVALID_ARG;
        }
    }

    sensors[sensor_count] = *sensor;
    sensors[sensor_count].priv_data = sensor->priv_data;

    if (sensors[sensor_count].init) {
        if (!sensors[sensor_count].init(&sensors[sensor_count])) {
            ESP_LOGE(TAG, "Failed to initialize sensor '%s'", sensor->name);
            return ESP_FAIL;
        }
    }

    sensors[sensor_count].enabled = true;
    sensor_count++;

    ESP_LOGI(TAG, "Registered sensor: %s (type=%d)", sensor->name, sensor->type);

    return ESP_OK;
}

esp_err_t sensor_manager_unregister(const char *name)
{
    for (int i = 0; i < sensor_count; i++) {
        if (strcmp(sensors[i].name, name) == 0) {
            if (sensors[i].deinit) {
                sensors[i].deinit(&sensors[i]);
            }
            for (int j = i; j < sensor_count - 1; j++) {
                sensors[j] = sensors[j + 1];
            }
            sensor_count--;
            ESP_LOGI(TAG, "Unregistered sensor: %s", name);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Sensor '%s' not found", name);
    return ESP_ERR_NOT_FOUND;
}

sensor_t *sensor_manager_find(const char *name)
{
    for (int i = 0; i < sensor_count; i++) {
        if (strcmp(sensors[i].name, name) == 0) {
            return &sensors[i];
        }
    }
    return NULL;
}

int sensor_manager_get_sensor_count(void)
{
    return sensor_count;
}

sensor_t *sensor_manager_get_sensor(int index)
{
    if (index < 0 || index >= sensor_count) {
        return NULL;
    }
    return &sensors[index];
}

esp_err_t sensor_manager_set_enabled(const char *name, bool enabled)
{
    sensor_t *sensor = sensor_manager_find(name);
    if (!sensor) {
        ESP_LOGE(TAG, "Sensor '%s' not found", name);
        return ESP_ERR_NOT_FOUND;
    }

    sensor->enabled = enabled;
    ESP_LOGI(TAG, "Sensor '%s' %s", name, enabled ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t sensor_manager_read(const char *name, sensor_data_t *data)
{
    sensor_t *sensor = sensor_manager_find(name);
    if (!sensor) {
        ESP_LOGE(TAG, "Sensor '%s' not found", name);
        return ESP_ERR_NOT_FOUND;
    }

    if (!sensor->enabled) {
        ESP_LOGW(TAG, "Sensor '%s' is disabled", name);
        return ESP_ERR_INVALID_STATE;
    }

    if (!sensor->read) {
        ESP_LOGE(TAG, "Sensor '%s' has no read function", name);
        return ESP_ERR_INVALID_ARG;
    }

    if (!sensor->read(sensor, data)) {
        ESP_LOGE(TAG, "Failed to read sensor '%s'", name);
        return ESP_FAIL;
    }

    if (data_cb) {
        data_cb(sensor->type, data);
    }

    return ESP_OK;
}

esp_err_t sensor_manager_read_all(void)
{
    for (int i = 0; i < sensor_count; i++) {
        if (sensors[i].enabled && sensors[i].read) {
            sensor_data_t data;
            if (sensors[i].read(&sensors[i], &data)) {
                if (data_cb) {
                    data_cb(sensors[i].type, &data);
                }
            }
        }
    }
    return ESP_OK;
}

void sensor_manager_register_data_cb(sensor_data_cb_t cb)
{
    data_cb = cb;
}