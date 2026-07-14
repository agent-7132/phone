#include "i2c_app.h"
#include "status_bar.h"
#include "app_manager.h"
#include "bsp/esp32_p4_platform.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "I2C_APP";

static lv_obj_t *result_text = NULL;
static lv_obj_t *scan_btn = NULL;

static void scan_i2c(lv_event_t *e)
{
    if (!result_text) return;
    
    lv_label_set_text(result_text, "Scanning...");
    
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = CONFIG_BSP_I2C_NUM,
        .scl_io_num = BSP_I2C_SCL,
        .sda_io_num = BSP_I2C_SDA,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus);
    
    if (ret != ESP_OK) {
        lv_label_set_text(result_text, "I2C bus create failed");
        return;
    }
    
    char result[512] = "I2C Devices:\n";
    int found = 0;
    
    for (uint8_t addr = 0x03; addr < 0x78; addr++) {
        i2c_master_dev_handle_t dev = NULL;
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        
        ret = i2c_master_bus_add_device(bus, &dev_config, &dev);
        
        if (ret == ESP_OK) {
            uint8_t dummy = 0;
            ret = i2c_master_transmit(dev, &dummy, 0, 100);
            
            if (ret == ESP_OK) {
                char addr_str[32];
                snprintf(addr_str, sizeof(addr_str), "0x%02X\n", addr);
                strcat(result, addr_str);
                found++;
            }
            
            i2c_master_bus_rm_device(dev);
        }
    }
    
    if (found == 0) {
        strcat(result, "No devices found");
    }
    
    lv_label_set_text(result_text, result);
    ESP_LOGI(TAG, "I2C scan completed, found %d devices", found);
}

static void back_button_cb(lv_event_t *e)
{
    app_manager_go_home();
}

lv_obj_t *i2c_app_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    status_bar_create(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "I2C Scanner");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    scan_btn = lv_btn_create(scr);
    lv_obj_set_size(scan_btn, 200, 50);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x4a4a6a), 0);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x5a5a8a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(scan_btn, 0, 0);
    lv_obj_set_style_radius(scan_btn, 8, 0);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_add_event_cb(scan_btn, scan_i2c, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(scan_btn);
    lv_label_set_text(btn_label, "Start Scan");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);

    lv_obj_t *container = lv_obj_create(scr);
    lv_obj_set_size(container, 400, 300);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x252540), 0);
    lv_obj_set_style_border_width(container, 1, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_radius(container, 10, 0);
    lv_obj_align(container, LV_ALIGN_CENTER, 0, 50);

    result_text = lv_label_create(container);
    lv_label_set_text(result_text, "Press scan button to start");
    lv_obj_set_style_text_font(result_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(result_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(result_text, 10, 0);
    lv_obj_set_size(result_text, 380, 280);
    lv_label_set_long_mode(result_text, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x3a3a5a), 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x4a4a6a), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 8, 0);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(back_btn, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "I2C app created");
    return scr;
}
