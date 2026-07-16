#include "gatt_service.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "os/os_mbuf.h"

static const char *TAG = "GATT_SERVICE";

#define MANUFACTURER_NAME "ESP32-P4 Phone"
#define FIRMWARE_VERSION "1.0.0"

static const ble_uuid16_t device_info_svc_uuid = BLE_UUID16_INIT(0x180A);
static const ble_uuid16_t manufacturer_name_chr_uuid = BLE_UUID16_INIT(0x2A29);
static const ble_uuid16_t firmware_version_chr_uuid = BLE_UUID16_INIT(0x2A26);



static int manufacturer_name_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;
    (void)conn_handle;
    (void)attr_handle;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    int rc = os_mbuf_append(ctxt->om, MANUFACTURER_NAME, strlen(MANUFACTURER_NAME));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int firmware_version_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)arg;
    (void)conn_handle;
    (void)attr_handle;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    int rc = os_mbuf_append(ctxt->om, FIRMWARE_VERSION, strlen(FIRMWARE_VERSION));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &device_info_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &manufacturer_name_chr_uuid.u,
                .access_cb = manufacturer_name_access,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = &firmware_version_chr_uuid.u,
                .access_cb = firmware_version_access,
                .flags = BLE_GATT_CHR_F_READ,
            },
            {0},
        },
    },
    {0},
};


esp_err_t gatt_service_init(void)
{
    ESP_LOGI(TAG, "Initializing GATT services...");

    int rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GATT services initialized");

    return ESP_OK;
}

void gatt_service_on_gap_event(struct ble_gap_event *event)
{
    (void)event;
}