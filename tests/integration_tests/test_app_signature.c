#include "unity.h"
#include "app_signature.h"
#include "esp_log.h"

static const char *TAG = "TEST_APP_SIGNATURE";

void setUp(void)
{
    app_signature_init();
}

void tearDown(void)
{
    app_signature_deinit();
}

void test_app_signature_init_deinit(void)
{
    ESP_LOGI(TAG, "Testing app_signature_init/deinit");
    
    esp_err_t ret = app_signature_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = app_signature_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_app_signature_status_to_string(void)
{
    ESP_LOGI(TAG, "Testing app_signature_status_to_string");
    
    TEST_ASSERT_EQUAL_STRING("OK", app_signature_status_to_string(SIGNATURE_STATUS_OK));
    TEST_ASSERT_EQUAL_STRING("Invalid", app_signature_status_to_string(SIGNATURE_STATUS_INVALID));
    TEST_ASSERT_EQUAL_STRING("Not Found", app_signature_status_to_string(SIGNATURE_STATUS_NOT_FOUND));
    TEST_ASSERT_EQUAL_STRING("Verify Failed", app_signature_status_to_string(SIGNATURE_STATUS_VERIFY_FAILED));
    TEST_ASSERT_EQUAL_STRING("Key Mismatch", app_signature_status_to_string(SIGNATURE_STATUS_KEY_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("Unknown", app_signature_status_to_string((app_signature_status_t)999));
}

void test_app_signature_load_invalid_path(void)
{
    ESP_LOGI(TAG, "Testing app_signature_load with invalid path");
    
    app_signature_t sig;
    esp_err_t ret = app_signature_load("/invalid/path/file.epp", &sig);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);
}

void test_app_signature_generate_verify_invalid_args(void)
{
    ESP_LOGI(TAG, "Testing app_signature_generate/verify with invalid args");
    
    app_signature_t sig;
    esp_err_t ret;
    
    ret = app_signature_generate(NULL, (uint8_t *)"key", 4, &sig);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = app_signature_generate("/test", NULL, 4, &sig);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    ret = app_signature_generate("/test", (uint8_t *)"key", 4, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    app_signature_status_t status = app_signature_verify(NULL, (uint8_t *)"key", 4);
    TEST_ASSERT_EQUAL(SIGNATURE_STATUS_INVALID, status);
    
    status = app_signature_verify("/test", NULL, 4);
    TEST_ASSERT_EQUAL(SIGNATURE_STATUS_INVALID, status);
    
    status = app_signature_verify("/test", (uint8_t *)"key", 0);
    TEST_ASSERT_EQUAL(SIGNATURE_STATUS_INVALID, status);
}