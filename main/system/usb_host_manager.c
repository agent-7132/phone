#include "usb_host_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/i2c_master.h"
#include "bsp/esp32_p4_platform.h"

static const char *TAG = "USB_HOST";

#define USB_HOST_TASK_STACK_SIZE 4096
#define USB_HOST_TASK_PRIORITY 5
#define USB_HOST_MOUNT_PATH "/usb"

#define AXP2101_ADDR            0x34
#define AXP2101_REG_USB_VBUS    0x1E

static usb_host_state_t s_state = USB_HOST_STATE_IDLE;
static usb_host_event_cb_t s_event_cb = NULL;
static msc_host_device_handle_t s_msc_dev = NULL;
static msc_host_vfs_handle_t s_msc_vfs_handle = NULL;
static TaskHandle_t s_usb_host_task = NULL;
static i2c_master_dev_handle_t s_axp2101_dev = NULL;
static volatile int s_file_operation_count = 0;
static SemaphoreHandle_t s_operation_mutex = NULL;

static void notify_event(usb_host_state_t state)
{
    s_state = state;
    if (s_event_cb) {
        s_event_cb(state, USB_HOST_MOUNT_PATH);
    }
}

static esp_err_t axp2101_init(void)
{
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP I2C");
        return ret;
    }
    
    i2c_master_bus_handle_t i2c_handle = bsp_i2c_get_handle();
    if (!i2c_handle) {
        ESP_LOGE(TAG, "BSP I2C handle is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    
    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDR,
        .scl_speed_hz = 100000,
    };
    
    ret = i2c_master_bus_add_device(i2c_handle, &dev_conf, &s_axp2101_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add AXP2101 device, VBUS control may not work: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "AXP2101 initialized via BSP I2C");
    return ESP_OK;
}

static esp_err_t axp2101_usb_vbus_set(bool enable)
{
    if (!s_axp2101_dev) {
        ESP_LOGW(TAG, "AXP2101 not initialized, skipping VBUS control");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t val = 0;
    esp_err_t ret = i2c_master_receive(s_axp2101_dev, &val, 1, pdMS_TO_TICKS(100));
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
    ret = i2c_master_transmit(s_axp2101_dev, &val, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AXP2101 USB VBUS %s (5V output)", enable ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG, "Failed to %s AXP2101 USB VBUS: %s", enable ? "enable" : "disable", esp_err_to_name(ret));
    }
    return ret;
}

static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    (void)arg;

    switch (event->event) {
        case MSC_DEVICE_CONNECTED:
            ESP_LOGI(TAG, "USB device connected, installing MSC driver");
            ESP_ERROR_CHECK(msc_host_install_device(event->device.address, &s_msc_dev));
            
            const esp_vfs_fat_mount_config_t mount_cfg = {
                .max_files = 5,
                .format_if_mount_failed = false,
            };
            esp_err_t ret = msc_host_vfs_register(s_msc_dev, USB_HOST_MOUNT_PATH, &mount_cfg, &s_msc_vfs_handle);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "USB disk mounted to %s", USB_HOST_MOUNT_PATH);
                notify_event(USB_HOST_STATE_MOUNTED);
            } else {
                ESP_LOGE(TAG, "Failed to mount USB disk: %s", esp_err_to_name(ret));
                msc_host_uninstall_device(s_msc_dev);
                s_msc_dev = NULL;
                s_msc_vfs_handle = NULL;
                notify_event(USB_HOST_STATE_ERROR);
            }
            break;

        case MSC_DEVICE_DISCONNECTED:
            ESP_LOGI(TAG, "USB device disconnected");
            if (s_msc_vfs_handle) {
                int op_count = usb_host_manager_get_file_operation_count();
                if (op_count > 0) {
                    ESP_LOGE(TAG, "Cannot unmount USB: %d file operations in progress", op_count);
                    notify_event(USB_HOST_STATE_ERROR);
                    break;
                }
                msc_host_vfs_unregister(s_msc_vfs_handle);
                msc_host_uninstall_device(s_msc_dev);
                s_msc_dev = NULL;
                s_msc_vfs_handle = NULL;
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

    s_operation_mutex = xSemaphoreCreateMutex();
    if (!s_operation_mutex) {
        ESP_LOGE(TAG, "Failed to create operation mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = axp2101_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 initialization failed, continuing without VBUS control: %s", esp_err_to_name(ret));
    }

    ret = axp2101_usb_vbus_set(true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Warning: Failed to enable AXP2101 USB VBUS, USB devices may not get power");
    }

    const usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
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

    const msc_host_driver_config_t msc_cfg = {
        .create_backround_task = false,
        .task_priority = USB_HOST_TASK_PRIORITY,
        .stack_size = USB_HOST_TASK_STACK_SIZE,
        .core_id = tskNO_AFFINITY,
        .callback = msc_event_cb,
        .callback_arg = NULL,
    };
    ret = msc_host_install(&msc_cfg);
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

    if (s_msc_vfs_handle) {
        msc_host_vfs_unregister(s_msc_vfs_handle);
        msc_host_uninstall_device(s_msc_dev);
        s_msc_dev = NULL;
        s_msc_vfs_handle = NULL;
    }

    msc_host_uninstall();

    if (s_usb_host_task) {
        vTaskDelete(s_usb_host_task);
        s_usb_host_task = NULL;
    }

    usb_host_uninstall();

    axp2101_usb_vbus_set(false);
    if (s_axp2101_dev) {
        i2c_master_bus_rm_device(s_axp2101_dev);
        s_axp2101_dev = NULL;
    }

    if (s_operation_mutex) {
        vSemaphoreDelete(s_operation_mutex);
        s_operation_mutex = NULL;
    }

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

void usb_host_manager_begin_file_operation(void)
{
    if (s_operation_mutex) {
        xSemaphoreTake(s_operation_mutex, portMAX_DELAY);
        s_file_operation_count++;
        xSemaphoreGive(s_operation_mutex);
    }
}

void usb_host_manager_end_file_operation(void)
{
    if (s_operation_mutex) {
        xSemaphoreTake(s_operation_mutex, portMAX_DELAY);
        if (s_file_operation_count > 0) {
            s_file_operation_count--;
        }
        xSemaphoreGive(s_operation_mutex);
    }
}

int usb_host_manager_get_file_operation_count(void)
{
    int count = 0;
    if (s_operation_mutex) {
        xSemaphoreTake(s_operation_mutex, portMAX_DELAY);
        count = s_file_operation_count;
        xSemaphoreGive(s_operation_mutex);
    }
    return count;
}