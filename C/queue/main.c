#include <stdio.h>
#include "queue.h"

#define QUEUE_CAPACITY 20

int queue_test_destroy_callback(uint8_t* data, uint32_t length)
{
    printf("Queue destroyed. Data length: %u\n", length);
    return 0;
}

int main()
{
    // 静态内存版：缓冲区由用户提供，需按 queue_alignment() 对齐
    static uint8_t storage[64]; // 64 字节，天然对齐，需 >= queue_sizeof(QUEUE_CAPACITY)

    if (sizeof(storage) < queue_sizeof(QUEUE_CAPACITY))
    {
        printf("Storage too small\n");
        return -1;
    }

    Queue_Struct* queue = queue_create((uint8_t*)storage, QUEUE_CAPACITY);
    if (queue == NULL)
    {
        printf("Failed to create queue\n");
        return -1;
    }

    // 注册销毁回调函数
    queue_register_destroy_callback(queue, queue_test_destroy_callback);

    uint8_t value_push[] = "Hello_World!!";
    uint32_t size = (uint32_t)(sizeof(value_push) / sizeof(value_push[0])) - 1; // 不含结尾 '\0'

    if (queue_push(queue, value_push, size) != 0)
    {
        printf("queue_push failed\n");
        return -1;
    }

    uint8_t value_pop[QUEUE_CAPACITY] = { 0 };
    if (queue_pop(queue, value_pop, 10) != 0)
    {
        printf("queue_pop failed\n");
        return -1;
    }
    value_pop[10] = '\0'; // 添加结尾 '\0' 以便打印字符串
    printf("Popped value: %s\n", (char*)value_pop);
    queue_destroy(queue);

    printf("Queue implementation in C\n");
    return 0;
}
