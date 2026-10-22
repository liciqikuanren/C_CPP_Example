#include "queue.h"


struct __Queue_Struct
{
    uint8_t* data;
    uint32_t head;
    uint32_t tail;
    uint32_t capacity;
};


Queue_Struct* queue_create(uint8_t* memory, uint32_t capacity)
{
    Queue_Struct* queue = memory;
    if (!queue)
    {
        return NULL; // Memory allocation failed
    }

    queue->data = (uint8_t*)malloc(capacity * sizeof(uint8_t));
    if (!queue->data)
    {
        free(queue);
        return NULL; // Memory allocation failed
    }

    queue->head = 0;
    queue->tail = 0;
    queue->capacity = capacity;

    return queue;

}

void queue_destroy(Queue_Struct* queue)
{
    free(queue->data);
    queue->data = NULL;
    queue->head = 0;
    queue->tail = 0;
    queue->capacity = 0;
}

int queue_push(Queue_Struct* queue, uint8_t value[], uint32_t size)
{

    if (size > (uint32_t)(queue->capacity - queue->tail))
    {
        return -1; // Not enough space to push
    }

    if (queue->tail == queue->capacity)
    {
        return -1; // Queue is full
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
    if (size > (uint32_t)(queue->tail - queue->head))
    {
        return -1; // Not enough elements to pop
    }

    if (queue->head == queue->tail)
    {
        return -1; // Queue is empty
    }

    for (uint32_t i = 0; i < size; i++)
    {
        value[i] = queue->data[queue->head + i];
    }
    queue->head += size;

    return 0; // Success
}