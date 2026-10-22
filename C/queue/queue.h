#ifndef _QUEUE_H__
#define _QUEUE_H__

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

/// @brief 队列结构体（不透明类型，定义在 queue.c 中）
typedef struct __Queue_Struct Queue_Struct;

/// @brief 计算一个队列实例所需的字节数（结构体 + 数据区）
/// @param capacity 数据区容量（字节数）
/// @return 调用方缓冲区至少需要的字节数
size_t queue_sizeof(uint32_t capacity);

/// @brief 获取队列结构体所需的对齐字节数
/// @return 传入 queue_create 的缓冲区地址必须是该值的整数倍
size_t queue_alignment(void);

/// @brief 用调用方提供的静态内存创建队列（内存归调用方所有，不会释放）
/// @param memory 缓冲区起始地址，大小必须 >= queue_sizeof(capacity)，
///               且地址需按 queue_alignment() 对齐
/// @param capacity 数据区容量（字节数），不能为 0
/// @return 返回指向队列的指针，参数非法（空指针/未对齐/容量为0/缓冲区不足）时返回NULL
Queue_Struct* queue_create(uint8_t* memory, uint32_t capacity);

/// @brief 重置队列（清空数据并回到初始状态，不释放内存）
/// @param queue 队列实例，可为 NULL
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
