#include "unity.h"
#include "utils.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_utils_strcasecmp(void)
{
    TEST_ASSERT_EQUAL(0, utils_strcasecmp("hello", "HELLO"));
    TEST_ASSERT_EQUAL(0, utils_strcasecmp("HELLO", "hello"));
    TEST_ASSERT_EQUAL(0, utils_strcasecmp("", ""));
    
    TEST_ASSERT_TRUE(utils_strcasecmp("apple", "banana") < 0);
    TEST_ASSERT_TRUE(utils_strcasecmp("banana", "apple") > 0);
    
    TEST_ASSERT_EQUAL(0, utils_strcasecmp(NULL, NULL));
    TEST_ASSERT_TRUE(utils_strcasecmp("test", NULL) > 0);
    TEST_ASSERT_TRUE(utils_strcasecmp(NULL, "test") < 0);
}

void test_utils_strcasestr(void)
{
    TEST_ASSERT_NOT_NULL(utils_strcasestr("Hello World", "world"));
    TEST_ASSERT_NOT_NULL(utils_strcasestr("Hello World", "HELLO"));
    TEST_ASSERT_NOT_NULL(utils_strcasestr("Hello World", "llo"));
    
    TEST_ASSERT_NULL(utils_strcasestr("Hello World", "xyz"));
    TEST_ASSERT_NULL(utils_strcasestr("Hello World", ""));
    TEST_ASSERT_NULL(utils_strcasestr("", "test"));
    TEST_ASSERT_NULL(utils_strcasestr(NULL, "test"));
    TEST_ASSERT_NULL(utils_strcasestr("test", NULL));
}

void test_utils_strdup(void)
{
    const char *original = "test string";
    char *copy = utils_strdup(original);
    
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_STRING(original, copy);
    
    free(copy);
    
    TEST_ASSERT_NULL(utils_strdup(NULL));
}

void test_utils_hash_string(void)
{
    uint32_t hash1 = utils_hash_string("test");
    uint32_t hash2 = utils_hash_string("test");
    uint32_t hash3 = utils_hash_string("TEST");
    
    TEST_ASSERT_EQUAL(hash1, hash2);
    TEST_ASSERT_EQUAL(hash1, hash3);
    
    TEST_ASSERT_EQUAL(0, utils_hash_string(NULL));
}

void test_utils_hash_table(void)
{
    hash_table_t table;
    utils_hash_table_init(&table);
    
    int val1 = 123;
    int val2 = 456;
    
    utils_hash_table_insert(&table, "key1", &val1);
    utils_hash_table_insert(&table, "key2", &val2);
    
    TEST_ASSERT_EQUAL(2, utils_hash_table_count(&table));
    
    int *result1 = (int *)utils_hash_table_lookup(&table, "key1");
    int *result2 = (int *)utils_hash_table_lookup(&table, "key2");
    int *result3 = (int *)utils_hash_table_lookup(&table, "key3");
    
    TEST_ASSERT_NOT_NULL(result1);
    TEST_ASSERT_NOT_NULL(result2);
    TEST_ASSERT_NULL(result3);
    
    TEST_ASSERT_EQUAL(123, *result1);
    TEST_ASSERT_EQUAL(456, *result2);
    
    bool removed = utils_hash_table_remove(&table, "key1");
    TEST_ASSERT_TRUE(removed);
    TEST_ASSERT_EQUAL(1, utils_hash_table_count(&table));
    
    utils_hash_table_clear(&table);
    TEST_ASSERT_EQUAL(0, utils_hash_table_count(&table));
}

void test_utils_sort_string_array(void)
{
    char *array[] = {"banana", "apple", "cherry", "date"};
    int count = 4;
    
    utils_sort_string_array(array, count);
    
    TEST_ASSERT_EQUAL_STRING("apple", array[0]);
    TEST_ASSERT_EQUAL_STRING("banana", array[1]);
    TEST_ASSERT_EQUAL_STRING("cherry", array[2]);
    TEST_ASSERT_EQUAL_STRING("date", array[3]);
}

void test_utils_binary_search_string(void)
{
    char *array[] = {"apple", "banana", "cherry", "date"};
    int count = 4;
    
    TEST_ASSERT_EQUAL(0, utils_binary_search_string(array, count, "apple"));
    TEST_ASSERT_EQUAL(1, utils_binary_search_string(array, count, "banana"));
    TEST_ASSERT_EQUAL(2, utils_binary_search_string(array, count, "cherry"));
    TEST_ASSERT_EQUAL(3, utils_binary_search_string(array, count, "date"));
    TEST_ASSERT_EQUAL(-1, utils_binary_search_string(array, count, "xyz"));
    
    TEST_ASSERT_EQUAL(-1, utils_binary_search_string(NULL, 0, "test"));
    TEST_ASSERT_EQUAL(-1, utils_binary_search_string(array, 0, "test"));
}