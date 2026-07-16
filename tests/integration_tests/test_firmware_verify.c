#include "unity.h"
#include "firmware_verify.h"
#include "esp_log.h"

static const char *TAG = "TEST_FIRMWARE_VERIFY";

void setUp(void)
{
    firmware_verify_init();
}

void tearDown(void)
{
    firmware_verify_deinit();
}

void test_firmware_verify_init_deinit(void)
{
    ESP_LOGI(TAG, "Testing firmware_verify_init/deinit");
    
    esp_err_t ret = firmware_verify_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = firmware_verify_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_firmware_verify_status_to_string(void)
{
    ESP_LOGI(TAG, "Testing firmware_verify_status_to_string");
    
    TEST_ASSERT_EQUAL_STRING("OK", firmware_verify_status_to_string(FIRMWARE_STATUS_OK));
    TEST_ASSERT_EQUAL_STRING("Invalid Header", firmware_verify_status_to_string(FIRMWARE_STATUS_INVALID_HEADER));
    TEST_ASSERT_EQUAL_STRING("Hash Mismatch", firmware_verify_status_to_string(FIRMWARE_STATUS_HASH_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("Too Large", firmware_verify_status_to_string(FIRMWARE_STATUS_TOO_LARGE));
    TEST_ASSERT_EQUAL_STRING("Corrupted", firmware_verify_status_to_string(FIRMWARE_STATUS_CORRUPTED));
    TEST_ASSERT_EQUAL_STRING("Unknown", firmware_verify_status_to_string((firmware_verify_status_t)999));
}

void test_firmware_verify_is_header_valid(void)
{
    ESP_LOGI(TAG, "Testing firmware_verify_is_header_valid");
    
    firmware_verify_header_t header;
    memset(&header, 0, sizeof(firmware_verify_header_t));
    
    TEST_ASSERT_FALSE(firmware_verify_is_header_valid(NULL));
    TEST_ASSERT_FALSE(firmware_verify_is_header_valid(&header));
    
    memcpy(header.magic, FIRMWARE_VERIFY_MAGIC, FIRMWARE_VERIFY_MAGIC_SIZE);
    header.version = FIRMWARE_VERIFY_VERSION;
    
    TEST_ASSERT_TRUE(firmware_verify_is_header_valid(&header));
    
    header.version = 999;
    TEST_ASSERT_FALSE(firmware_verify_is_header_valid(&header));
}

void test_firmware_verify_check_invalid_args(void)
{
    ESP_LOGI(TAG, "Testing firmware_verify_check with invalid args");
    
    firmware_verify_status_t status;
    
    status = firmware_verify_check(NULL, 0);
    TEST_ASSERT_EQUAL(FIRMWARE_STATUS_INVALID_HEADER, status);
    
    uint8_t data[10];
    status = firmware_verify_check(data, 0);
    TEST_ASSERT_EQUAL(FIRMWARE_STATUS_INVALID_HEADER, status);
    
    status = firmware_verify_check(data, sizeof(data));
    TEST_ASSERT_EQUAL(FIRMWARE_STATUS_CORRUPTED, status);
}

void test_firmware_verify_generate_header_invalid_args(void)
{
    ESP_LOGI(TAG, "Testing firmware_verify_generate_header with invalid args");
    
    firmware_verify_header_t header;
    uint8_t data[10];
    
    esp_err_t ret;
    
    ret = firmware_verify_generate_header(NULL, 10, &header);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = firmware_verify_generate_header(data, 0, &header);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = firmware_verify_generate_header(data, 10, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}