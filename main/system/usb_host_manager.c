#if CONFIG_BSP_USB_HOST_ENABLE

#include "usb_host_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_host.h"
#include "usb_msc.h"
#include "esp_vfs_fat.h"
#include "driver/i2c.h"

static const char *TAG = "USB_HOST";

#define USB_HOST_TASK_STACK_SIZE 4096
#define USB_HOST_TASK_PRIORITY 5
#define USB_HOST_MOUNT_PATH "/usb"

#define AXP2101_I2C_NUM         I2C_NUM_0
#define AXP2101_I2C_SDA_GPIO    GPIO_NUM_8
#define AXP2101_I2C_SCL_GPIO    GPIO_NUM_9
#define AXP2101_ADDR            0x34
#define AXP2101_REG_USB_VBUS    0x1E

static usb_host_state_t s_state = USB_HOST_STATE_IDLE;
static usb_host_event_cb_t s_event_cb = NULL;
static usb_msc_handle_t s_msc_handle = NULL;
static usb_msc_device_handle_t s_msc_dev = NULL;
static TaskHandle_t s_usb_host_task = NULL;

static void notify_event(usb_host_state_t state)
{
    s_state = state;
    if (s_event_cb) {
        s_event_cb(state, USB_HOST_MOUNT_PATH);
    }
}

static esp_err_t axp2101_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = AXP2101_I2C_SDA_GPIO,
        .scl_io_num = AXP2101_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(AXP2101_I2C_NUM, &conf), TAG, "I2C config fail");
    ESP_RETURN_ON_ERROR(i2c_driver_install(AXP2101_I2C_NUM, conf.mode, 0, 0, 0), TAG, "I2C install fail");
    ESP_LOGI(TAG, "AXP2101 I2C initialized (SDA:%d, SCL:%d)", AXP2101_I2C_SDA_GPIO, AXP2101_I2C_SCL_GPIO);
    return ESP_OK;
}

static esp_err_t axp2101_usb_vbus_set(bool enable)
{
    uint8_t val = 0;
    esp_err_t ret = i2c_master_read_from_device(AXP2101_I2C_NUM, AXP2101_ADDR, &val, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read AXP2101 USB_VBUS register, trying write directly");
        val = enable ? 0x01 : 0x00;
    } else {
        if (enable) {
            val |= BIT(0);
        } else {
            val &= ~BIT(0);
        }
    }
    ret = i2c_master_write_to_device(AXP2101_I2C_NUM, AXP2101_ADDR, &val, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AXP2101 USB VBUS %s (5V output)", enable ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG, "Failed to %s AXP2101 USB VBUS", enable ? "enable" : "disable");
    }
    return ret;
}

static void msc_event_cb(usb_msc_handle_t msc_handle, const usb_msc_event_t *event, void *user_data)
{
    (void)msc_handle;
    (void)user_data;

    switch (event->event_type) {
        case MSC_DEVICE_CONNECTED:
            ESP_LOGI(TAG, "USB device connected, installing MSC driver");
            ESP_ERROR_CHECK(usb_msc_install_device(s_msc_handle, &s_msc_dev));
            
            const esp_vfs_fat_mount_config_t mount_cfg = {
                .max_files = 5,
                .format_if_mount_failed = false,
            };
            esp_err_t ret = usb_msc_vfs_register(s_msc_dev, USB_HOST_MOUNT_PATH, &mount_cfg);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "USB disk mounted to %s", USB_HOST_MOUNT_PATH);
                notify_event(USB_HOST_STATE_MOUNTED);
            } else {
                ESP_LOGE(TAG, "Failed to mount USB disk: %s", esp_err_to_name(ret));
                usb_msc_uninstall_device(s_msc_dev);
                s_msc_dev = NULL;
                notify_event(USB_HOST_STATE_ERROR);
            }
            break;

        case MSC_DEVICE_DISCONNECTED:
            ESP_LOGI(TAG, "USB device disconnected");
            if (s_msc_dev) {
                usb_msc_vfs_unregister(s_msc_dev);
                usb_msc_uninstall_device(s_msc_dev);
                s_msc_dev = NULL;
            }
            notify_event(USB_HOST_STATE_INITIALIZED);
            break;

        default:
            break;
    }
}

static void usb_host_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "USB host task started");
    while (1) {
        usb_host_lib_handle_events(pdMS_TO_TICKS(10), NULL);
    }
    vTaskDelete(NULL);
}

esp_err_t usb_host_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing USB host manager...");

    if (s_state != USB_HOST_STATE_IDLE) {
        ESP_LOGW(TAG, "USB host already initialized");
        return ESP_OK;
    }

    esp_err_t ret = axp2101_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AXP2101 I2C");
        return ret;
    }

    ret = axp2101_usb_vbus_set(true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Warning: Failed to enable AXP2101 USB VBUS, USB devices may not get power");
    }

    const usb_host_config_t host_cfg = USB_HOST_CONFIG_DEFAULT();
    ret = usb_host_install(&host_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB host: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "USB host stack installed");

    BaseType_t task_ret = xTaskCreate(usb_host_task, "usb_host_task", 
                                      USB_HOST_TASK_STACK_SIZE, NULL, USB_HOST_TASK_PRIORITY, 
                                      &s_usb_host_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB host task");
        usb_host_uninstall();
        return ESP_ERR_NO_MEM;
    }

    const usb_msc_config_t msc_cfg = {
        .base_path = USB_HOST_MOUNT_PATH,
        .event_callback = msc_event_cb,
    };
    ret = usb_msc_install(&msc_cfg, &s_msc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install MSC driver: %s", esp_err_to_name(ret));
        vTaskDelete(s_usb_host_task);
        usb_host_uninstall();
        return ret;
    }
    ESP_LOGI(TAG, "MSC driver installed, waiting for USB device...");

    s_state = USB_HOST_STATE_INITIALIZED;
    notify_event(USB_HOST_STATE_INITIALIZED);
    return ESP_OK;
}

esp_err_t usb_host_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing USB host manager...");

    if (s_msc_dev) {
        usb_msc_vfs_unregister(s_msc_dev);
        usb_msc_uninstall_device(s_msc_dev);
        s_msc_dev = NULL;
    }

    if (s_msc_handle) {
        usb_msc_uninstall(s_msc_handle);
        s_msc_handle = NULL;
    }

    if (s_usb_host_task) {
        vTaskDelete(s_usb_host_task);
        s_usb_host_task = NULL;
    }

    usb_host_uninstall();

    axp2101_usb_vbus_set(false);
    i2c_driver_delete(AXP2101_I2C_NUM);

    s_state = USB_HOST_STATE_IDLE;
    ESP_LOGI(TAG, "USB host manager deinitialized");
    return ESP_OK;
}

usb_host_state_t usb_host_manager_get_state(void)
{
    return s_state;
}

const char *usb_host_manager_get_mount_path(void)
{
    return USB_HOST_MOUNT_PATH;
}

void usb_host_manager_register_event_cb(usb_host_event_cb_t cb)
{
    s_event_cb = cb;
}

bool usb_host_manager_is_mounted(void)
{
    return s_state == USB_HOST_STATE_MOUNTED;
}

#else

#include "usb_host_manager.h"
#include "esp_log.h"

static const char *TAG = "USB_HOST";

esp_err_t usb_host_manager_init(void)
{
    ESP_LOGI(TAG, "USB host support is disabled (CONFIG_BSP_USB_HOST_ENABLE not set)");
    return ESP_OK;
}

esp_err_t usb_host_manager_deinit(void)
{
    return ESP_OK;
}

usb_host_state_t usb_host_manager_get_state(void)
{
    return USB_HOST_STATE_IDLE;
}

const char *usb_host_manager_get_mount_path(void)
{
    return NULL;
}

void usb_host_manager_register_event_cb(usb_host_event_cb_t cb)
{
    (void)cb;
}

bool usb_host_manager_is_mounted(void)
{
    return false;
}

#endif