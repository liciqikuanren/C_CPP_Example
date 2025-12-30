/* event.h
 * 纯软件版事件系统头文件
 * 适用于 PC 上运行的 C 程序，无任何嵌入式依赖
 */

#ifndef __EVENT_H
#define __EVENT_H

#include <stdint.h>

/* ==================== 配置宏 ==================== */
// 用户可根据需要修改以下值
#define EVENT_MAX_COUNT         32    // 支持的最大事件类型数量
#define EVENT_SUBSCRIBER_MAX    8     // 每个事件类型最多订阅者数量
#define EVENT_OBSERVER_MAX      4     // 全局观察者最大数量
#define EVENT_QUEUE_SIZE        64    // 事件队列深度
#define EVENT_DATA_SIZE_MAX     32    // 事件携带数据最大字节数
#define EVENT_DEBUG_ENABLE      1     // 1=开启调试打印，0=关闭

/* ==================== 类型定义 ==================== */
typedef uint16_t Event_Type_t;
typedef uint8_t  Event_Priority_t;

typedef struct {
    Event_Type_t type;                   /* 事件类型 */
    Event_Priority_t priority;           /* 事件优先级（暂未使用，可扩展） */
    uint32_t timestamp;                  /* 事件时间戳（ms） */
    uint8_t data_size;                   /* 数据大小 */
    uint8_t data[EVENT_DATA_SIZE_MAX];   /* 事件数据 */
} Event_t;

/* 事件回调函数类型 */
typedef void (*EventCallback_t)(Event_t* event, void* arg);

/* ==================== 公共API ==================== */
int EVENT_Init(void);

int EVENT_Subscribe(Event_Type_t type, EventCallback_t callback, void* arg);
int EVENT_Unsubscribe(Event_Type_t type, EventCallback_t callback, void* arg);

int EVENT_Publish(Event_Type_t type, Event_Priority_t priority,
                  const void* data, uint8_t data_size);

int EVENT_Process(void);                // 返回本次处理的事件数量

int EVENT_ClearQueue(void);
uint16_t EVENT_GetCount(void);

int EVENT_RegisterObserver(EventCallback_t callback, void* arg);
int EVENT_UnregisterObserver(EventCallback_t callback);

#endif /* __EVENT_H */
