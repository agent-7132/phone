#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t gatt_service_init(void);
void gatt_service_on_gap_event(struct ble_gap_event *event);

#ifdef __cplusplus
}
#endif