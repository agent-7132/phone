#include "unity.h"
#include "ui_utils.h"
#include "lvgl.h"

static lv_obj_t *scr = NULL;

void setUp(void)
{
    scr = lv_obj_create(NULL);
}

void tearDown(void)
{
    if (scr) {
        lv_obj_del(scr);
        scr = NULL;
    }
}

void test_ui_utils_create_screen(void)
{
    lv_obj_t *screen = ui_utils_create_screen();
    TEST_ASSERT_NOT_NULL(screen);
    lv_obj_del(screen);
}

void test_ui_utils_create_container_default(void)
{
    lv_obj_t *container = ui_utils_create_container_default(scr, 100, 100);
    TEST_ASSERT_NOT_NULL(container);
    
    lv_coord_t w = lv_obj_get_width(container);
    lv_coord_t h = lv_obj_get_height(container);
    TEST_ASSERT_EQUAL(100, w);
    TEST_ASSERT_EQUAL(100, h);
    
    lv_obj_del(container);
}

void test_ui_utils_create_button_default(void)
{
    lv_obj_t *btn = ui_utils_create_button_default(scr, "Test", NULL, NULL);
    TEST_ASSERT_NOT_NULL(btn);
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    TEST_ASSERT_NOT_NULL(label);
    TEST_ASSERT_TRUE(lv_obj_check_type(label, &lv_label_class));
    
    lv_obj_del(btn);
}

void test_ui_utils_create_title(void)
{
    lv_obj_t *title = ui_utils_create_title(scr, "Test Title");
    TEST_ASSERT_NOT_NULL(title);
    
    const char *text = lv_label_get_text(title);
    TEST_ASSERT_EQUAL_STRING("Test Title", text);
    
    lv_obj_del(title);
}

void test_ui_utils_create_label_default(void)
{
    lv_obj_t *label = ui_utils_create_label_default(scr, "Test Label");
    TEST_ASSERT_NOT_NULL(label);
    
    const char *text = lv_label_get_text(label);
    TEST_ASSERT_EQUAL_STRING("Test Label", text);
    
    lv_obj_del(label);
}

void test_ui_utils_create_back_button(void)
{
    lv_obj_t *btn = ui_utils_create_back_button(scr);
    TEST_ASSERT_NOT_NULL(btn);
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    TEST_ASSERT_NOT_NULL(label);
    
    lv_obj_del(btn);
}

void test_ui_utils_set_button_text(void)
{
    lv_obj_t *btn = ui_utils_create_button_default(scr, "Original", NULL, NULL);
    TEST_ASSERT_NOT_NULL(btn);
    
    ui_utils_set_button_text(btn, "Changed");
    
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    const char *text = lv_label_get_text(label);
    TEST_ASSERT_EQUAL_STRING("Changed", text);
    
    lv_obj_del(btn);
}