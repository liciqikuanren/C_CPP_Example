#include <stdio.h>
#include "queue.h"


int main() {
    uint8_t queue_memory[20];
    Queue_Struct* queue = queue_create(queue_memory, sizeof(queue_memory));
    if (queue == NULL) {
        printf("Failed to create queue\n");
        return -1;
    }

    uint8_t value_push[] = "Hello_World!!";
    uint32_t size = (uint32_t)(sizeof(value_push) / sizeof(value_push[0])) - 1; // 不含结尾 '\0'
    queue_push(queue, value_push, size);
    uint8_t value_pop[20] = {0};
    queue_pop(queue, value_pop, size);
    printf("Popped value: %s\n", (char*)value_pop);
    printf("Queue implementation in C\n");
    return 0;
}