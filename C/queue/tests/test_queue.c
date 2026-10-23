#include "unity.h"
#include "queue.h"

#define TEST_CAPACITY 32

static uint32_t storage[64];
static Queue_Struct* queue;

void setUp(void)
{
    TEST_ASSERT_TRUE(sizeof(storage) >= queue_sizeof(TEST_CAPACITY));
    queue = queue_create((uint8_t*)storage, TEST_CAPACITY);
    TEST_ASSERT_NOT_NULL(queue);
}

void tearDown(void)
{
    queue_destroy(queue);
    queue = NULL;
}

static void test_sizeof_includes_header_and_data(void)
{
    TEST_ASSERT_TRUE(queue_sizeof(0) > 0);
    TEST_ASSERT_EQUAL_size_t(queue_sizeof(10) + TEST_CAPACITY - 10, queue_sizeof(TEST_CAPACITY));
    TEST_ASSERT_TRUE(queue_alignment() > 0);
}

static void test_create_rejects_invalid_args(void)
{
    TEST_ASSERT_NULL(queue_create(NULL, TEST_CAPACITY));
    TEST_ASSERT_NULL(queue_create((uint8_t*)storage, 0));
    TEST_ASSERT_NULL(queue_create((uint8_t*)storage + 1, TEST_CAPACITY));
}

static void test_push_and_pop_single_byte(void)
{
    uint8_t in = 0x5A;
    uint8_t out = 0;

    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, &in, 1));
    TEST_ASSERT_EQUAL_INT(0, queue_pop(queue, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(0x5A, out);
}

static void test_fifo_order(void)
{
    const uint8_t values[] = { 1, 2, 3, 4 };
    uint8_t out = 0;

    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, values, sizeof(values)));

    for (size_t i = 0; i < sizeof(values); i++)
    {
        TEST_ASSERT_EQUAL_INT(0, queue_pop(queue, &out, 1));
        TEST_ASSERT_EQUAL_UINT8(values[i], out);
    }
}

static void test_pop_empty_fails(void)
{
    uint8_t out = 0;

    TEST_ASSERT_EQUAL_INT(-1, queue_pop(queue, &out, 1));
}

static void test_push_beyond_capacity_fails(void)
{
    uint8_t block[TEST_CAPACITY] = { 0 };
    uint8_t extra = 0xAA;

    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, block, TEST_CAPACITY));
    TEST_ASSERT_EQUAL_INT(-1, queue_push(queue, &extra, 1));
}

static void test_pop_more_than_available_fails(void)
{
    uint8_t in[] = { 1, 2, 3 };
    uint8_t out[8] = { 0 };

    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, in, sizeof(in)));
    TEST_ASSERT_EQUAL_INT(-1, queue_pop(queue, out, sizeof(out)));
}

static void test_null_args_fail(void)
{
    uint8_t value = 0;

    TEST_ASSERT_EQUAL_INT(-1, queue_push(NULL, &value, 1));
    TEST_ASSERT_EQUAL_INT(-1, queue_push(queue, NULL, 1));
    TEST_ASSERT_EQUAL_INT(-1, queue_pop(NULL, &value, 1));
    TEST_ASSERT_EQUAL_INT(-1, queue_pop(queue, NULL, 1));
}

static void test_destroy_resets_queue(void)
{
    uint8_t in[] = { 1, 2, 3 };
    uint8_t out = 0;

    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, in, sizeof(in)));
    queue_destroy(queue);

    TEST_ASSERT_EQUAL_INT(-1, queue_pop(queue, &out, 1));
    TEST_ASSERT_EQUAL_INT(-1, queue_push(queue, in, sizeof(in))); // capacity 已被重置为 0

    // 重新创建后可继续使用
    queue = queue_create((uint8_t*)storage, TEST_CAPACITY);
    TEST_ASSERT_NOT_NULL(queue);
    TEST_ASSERT_EQUAL_INT(0, queue_push(queue, in, sizeof(in)));
    TEST_ASSERT_EQUAL_INT(0, queue_pop(queue, &out, 1));
    TEST_ASSERT_EQUAL_UINT8(1, out);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sizeof_includes_header_and_data);
    RUN_TEST(test_create_rejects_invalid_args);
    RUN_TEST(test_push_and_pop_single_byte);
    RUN_TEST(test_fifo_order);
    RUN_TEST(test_pop_empty_fails);
    RUN_TEST(test_push_beyond_capacity_fails);
    RUN_TEST(test_pop_more_than_available_fails);
    RUN_TEST(test_null_args_fail);
    RUN_TEST(test_destroy_resets_queue);
    return UNITY_END();
}
