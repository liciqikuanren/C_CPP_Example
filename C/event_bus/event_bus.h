#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- 可配置项（编译期宏，可在包含本头文件前重新定义） ---------------- */
#ifndef EVENT_PAYLOAD_MAX
#define EVENT_PAYLOAD_MAX   16u             /* 事件载荷最大字节数 */
#endif

#ifndef PUB_QUEUE_LEN
#define PUB_QUEUE_LEN       8u              /* 发布者事件队列深度 */
#endif

#ifndef SUB_QUEUE_LEN
#define SUB_QUEUE_LEN       8u              /* 订阅者缓存队列深度 */
#endif

#ifndef EVENT_BUS_MAX_PUBS
#define EVENT_BUS_MAX_PUBS  8u              /* 引擎最多挂载的发布者数量 */
#endif

/* 临界区钩子：多线程/中断环境下自行填入关中断或加锁，默认空操作 */
#ifndef EVENT_BUS_LOCK
#define EVENT_BUS_LOCK()
#endif
#ifndef EVENT_BUS_UNLOCK
#define EVENT_BUS_UNLOCK()
#endif
/* ------------------------------------------------------------------------ */

/* 事件枚举 ID：发布者用它标识自己，订阅者用它倒推发布者 */
typedef enum {
    EVENT_ID_NONE = 0,                      /* 空事件 */
    EVENT_ID_KEY,                           /* 按键事件 */
    EVENT_ID_TIMER,                         /* 定时器事件 */
    EVENT_ID_UART,                          /* 串口数据到达 */
    EVENT_ID_SENSOR,                        /* 传感器采样 */
    EVENT_ID_MAX                            /* 事件数量上界 */
} event_id_t;

#define EVENT_ID_ANY    ((event_id_t)0xFF)  /* 特殊值：订阅全部事件（挂到总线通配链表） */

/* 事件对象：ID + 有效长度 + 载荷 */
typedef struct event {
    event_id_t  id;                         /* 事件枚举 ID */
    uint16_t    len;                        /* 载荷有效字节数 */
    uint8_t     payload[EVENT_PAYLOAD_MAX]; /* 载荷数据 */
} event_t;

/* 事件处理回调：引擎从订阅者队列取出事件后调用 */
typedef void (*event_handler_t)(const event_t *ev);

/* ==================== 订阅者 ====================
 * 自身维护一个缓存队列，用于暂存分发过来的事件；
 * 通过 next 指针串联成单向链表，供引擎遍历；
 * 订阅动作由订阅者发起：传入事件 ID，由引擎倒推出对应发布者，
 * 再把本订阅者挂入该发布者的订阅者链表。
 */
typedef struct subscriber {
    event_handler_t     handler;            /* 事件回调函数 */

    event_t             queue[SUB_QUEUE_LEN];   /* 本地缓存队列 */
    uint8_t             head;               /* 队头下标（出队方向） */
    uint8_t             tail;               /* 队尾下标（入队方向） */
    uint8_t             count;              /* 队列中事件个数 */

    struct subscriber  *next;               /* 下一个订阅者地址，链表访问用 */
} subscriber_t;

/* ==================== 发布者 ====================
 * 持有枚举 ID、事件队列，以及订阅者链表的首地址（用于遍历订阅者）。
 * 订阅者链表中只挂“订阅了本发布者事件 ID”的订阅者。
 */
typedef struct publisher {
    event_id_t          id;                 /* 枚举 ID，订阅者据此倒推本发布者 */

    event_t             queue[PUB_QUEUE_LEN];   /* 事件队列 */
    uint8_t             head;               /* 队头下标 */
    uint8_t             tail;               /* 队尾下标 */
    uint8_t             count;              /* 队列中事件个数 */

    struct subscriber  *subs;               /* 订阅者链表首地址，遍历起点 */
} publisher_t;

/* ==================== 事件总线处理引擎 ====================
 * 维护已挂载的发布者表，承担“事件 ID -> 发布者”的倒推职责；
 * 另持有通配链表首地址，存放以 EVENT_ID_ANY 订阅的“万能订阅者”；
 * 由 event_bus_process() 驱动事件分发。
 */
typedef struct event_bus {
    publisher_t        *pubs[EVENT_BUS_MAX_PUBS];  /* 发布者指针表 */
    uint8_t             pub_cnt;            /* 已挂载的发布者数量 */
    struct subscriber  *any_subs;           /* 通配（EVENT_ID_ANY）订阅者链表首地址 */
} event_bus_t;

/* ---------- 订阅者接口（订阅动作的入口，按事件倒推发布者） ----------
 * subscriber_init()        只绑定回调，不挂链；
 * subscriber_subscribe()   按事件 ID 倒查发布者并挂入其链表；
 *                          传 EVENT_ID_ANY 则挂到总线通配链表（收所有事件）；
 *                          重复订阅会先自动摘旧链再挂新链（一个订阅者同一时刻
 *                          只属于一条链表，next 指针唯一）；
 * subscriber_unsubscribe() 按事件 ID 倒查发布者并从其链表摘除；
 *                          传 EVENT_ID_ANY 表示从所有链表全部摘除。
 * 需要同时接收多类事件时，请使用多个订阅者对象，或直接用 EVENT_ID_ANY。
 */
void subscriber_init(subscriber_t *sub, event_handler_t handler);
bool subscriber_subscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id);
void subscriber_unsubscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id);
bool subscriber_push(subscriber_t *sub, const event_t *ev);
bool subscriber_pop(subscriber_t *sub, event_t *out);
bool subscriber_empty(const subscriber_t *sub);

/* ---------- 发布者接口 ----------
 * 发布事件只需给出发布者实例、数据、数据长度：
 * 事件 ID 由发布者自身的枚举 ID 自动填入，调用方无需关心。
 * publisher_subscribe/unsubscribe 为底层挂链/摘链原语，
 * 一般业务代码请使用 subscriber_subscribe/subscriber_unsubscribe。
 */
void publisher_init(publisher_t *pub, event_id_t id);
bool publisher_post(publisher_t *pub, const void *data, uint16_t len);
void publisher_subscribe(publisher_t *pub, subscriber_t *sub);
void publisher_unsubscribe(publisher_t *pub, subscriber_t *sub);
bool publisher_empty(const publisher_t *pub);

/* ---------- 引擎接口 ---------- */
void event_bus_init(event_bus_t *bus);
bool event_bus_add_publisher(event_bus_t *bus, publisher_t *pub);
void event_bus_process(event_bus_t *bus);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUS_H */
