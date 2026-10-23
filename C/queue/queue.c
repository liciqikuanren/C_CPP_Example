#include "queue.h"


struct __Queue_Struct
{
    uint8_t* data;
    uint32_t head;
    uint32_t tail;
    uint32_t capacity;
    queue_destroy_callback destroy_callback;
};

typedef struct
{
    char _c;
    Queue_Struct _q;
} Queue_AlignProbe;

#define QUEUE_ALIGNMENT ((size_t)offsetof(Queue_AlignProbe, _q))


size_t queue_sizeof(uint32_t capacity)
{
    return sizeof(Queue_Struct) + (size_t)capacity;
}

size_t queue_alignment(void)
{
    return QUEUE_ALIGNMENT;
}

Queue_Struct* queue_create(uint8_t* memory, uint32_t capacity)
{
    if (memory == NULL || capacity == 0)
    {
        return NULL;
    }

    if ((uintptr_t)memory % QUEUE_ALIGNMENT != 0)
    {
        return NULL; // 缓冲区未按 Queue_Struct 要求对齐
    }

    Queue_Struct* queue = (Queue_Struct*)memory;
    queue->data = memory + sizeof(Queue_Struct);
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = capacity;

    return queue;
}

void queue_destroy(Queue_Struct* queue)
{
    if (!queue)
    {
        return;
    }

    queue->data = NULL;
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = 0;
    if(queue->destroy_callback!= NULL)
    {
        queue->destroy_callback(queue->data, queue->tail - queue->head);
    }
}

int queue_push(Queue_Struct* queue, const uint8_t value[], uint32_t size)
{
    if (!queue || !value)
    {
        return -1;
    }

    if (size > queue->capacity - queue->tail)
    {
        return -1; // Not enough space to push
    }

    for (uint32_t i = 0; i < size; i++)
    {
        queue->data[queue->tail + i] = value[i];
    }
    queue->tail += size;

    return 0; // Success
}


int queue_pop(Queue_Struct* queue, uint8_t value[], uint32_t size)
{
    if (!queue || !value)
    {
        return -1;
    }

    if (size > queue->tail - queue->head)
    {
        return -1; // Not enough elements to pop
    }

    for (uint32_t i = 0; i < size; i++)
    {
        value[i] = queue->data[queue->head + i];
    }
    queue->head += size;

    return 0; // Success
}



int queue_register_destroy_callback(Queue_Struct* queue, queue_destroy_callback callback)
{
    if (!queue)
    {
        return -1;
    }

    queue->destroy_callback = callback;
    return 0;
}