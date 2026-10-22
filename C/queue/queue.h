#ifndef _QUEUE_H__
#define _QUEUE_H__

#include <stdlib.h>
#include <stdint.h>
/// @brief 队列结构体（不透明类型，定义在 queue.c 中）
typedef struct __Queue_Struct Queue_Struct;

/// @brief 创建一个队列
/// @param memory 用于存储队列结构体的内存空间
/// @param capacity 容量
/// @return 返回一个指向队列的指针，如果创建失败则返回NULL
Queue_Struct* queue_create(uint8_t* memory, uint32_t capacity);

/// @brief 销毁队列
/// @param queue 队列实例
void queue_destroy(Queue_Struct* queue);

/// @brief 向队列中添加元素
/// @param queue 队列实例
/// @param value 要添加的元素数组
/// @param size 要添加的元素个数
/// @return 返回0表示成功，返回-1表示失败
int queue_push(Queue_Struct* queue, uint8_t value[], uint32_t size);

/// @brief 从队列中取出元素
/// @param queue 队列实例
/// @param value 用于存储取出元素的数组
/// @param size 要取出的元素个数
/// @return 返回0表示成功，返回-1表示失败

int queue_pop(Queue_Struct* queue, uint8_t value[], uint32_t size);

#endif


