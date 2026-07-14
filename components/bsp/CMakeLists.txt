if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.5")
    set(REQ esp_driver_gpio esp_driver_i2s esp_driver_sdmmc esp_driver_sdspi esp_driver_i2c)
    set(PRIV_REQ esp_driver_spi esp_driver_ledc)
else()
    set(REQ driver)
    set(PRIV_REQ "")
endif()

idf_component_register(
    SRCS "esp32_p4_platform.c" "../bsp_st7102/bsp.c"
    INCLUDE_DIRS "include" "../bsp_st7102/include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES ${REQ} fatfs
    PRIV_REQUIRES ${PRIV_REQ} esp_lcd spiffs esp_psram esp_lvgl_port esp_lcd_st7102 esp_lcd_touch_st7123
)