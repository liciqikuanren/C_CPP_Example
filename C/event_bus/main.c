/**
 * event_bus 使用示例
 * 演示：发布者（枚举 ID + 订阅者链表首地址 + 所属引擎，不自持队列）
 *       订阅者（缓存队列深度 1 + next 指针，按事件 ID 倒推发布者完成订阅）
 *       引擎  （允许多实例，每个实例一份事件总队列，扫描即回调）
 */
#include "event_bus.h"
#include <stdio.h>
#include <string.h>

/* 按键事件回调 */
static void on_key(const event_t *ev)
{
    printf("[sub_key ] id=%d len=%u data='%c'\n",
           ev->id, ev->len, ev->len ? (char)ev->payload[0] : '-');
}

/* 定时器事件回调：载荷为 uint32_t 计数值 */
static void on_timer(const event_t *ev)
{
    uint32_t tick;
    memcpy(&tick, ev->payload, sizeof(tick));
    printf("[sub_tick] id=%d tick=%u\n", ev->id, tick);
}

/* 万能日志回调：以 EVENT_ID_ANY 订阅，监听所在引擎的所有事件 */
static void on_log(const event_t *ev)
{
    printf("[sub_log ] id=%d len=%u\n", ev->id, ev->len);
}

/* 传感器回调：用于第二个引擎实例的演示 */
static void on_sensor(const event_t *ev)
{
    printf("[sub_sens] id=%d len=%u (bus2)\n", ev->id, ev->len);
}

int main(void)
{
    static event_bus_t  bus;                /* 引擎实例 1（静态分配） */
    publisher_t  key_pub;                    /* 发布者：枚举 ID + 订阅者链表首地址 */
    publisher_t  tick_pub;
    subscriber_t s_key;                      /* 订阅者：深度1缓存队列 + next 指针 */
    subscriber_t s_tick;
    subscriber_t s_log;
    uint32_t     tick = 0;

    /* 1. 初始化引擎，初始化发布者并挂载（挂载后才能被事件 ID 倒推查到，
     *    同时记录发布者归属，post 据此投递到本引擎总队列） */
    event_bus_init(&bus);

    publisher_init(&key_pub,  EVENT_ID_KEY);
    publisher_init(&tick_pub, EVENT_ID_TIMER);

    event_bus_add_publisher(&bus, &key_pub);
    event_bus_add_publisher(&bus, &tick_pub);

    /* 2. 初始化订阅者：只绑定回调，不挂链 */
    subscriber_init(&s_key,  on_key);
    subscriber_init(&s_tick, on_timer);
    subscriber_init(&s_log,  on_log);

    /* 3. 订阅（订阅者发起）：传事件 ID，由引擎倒推出对应发布者再挂入其链表 */
    subscriber_subscribe(&bus, &s_key,  EVENT_ID_KEY);    /* 倒推 -> key_pub  */
    subscriber_subscribe(&bus, &s_tick, EVENT_ID_TIMER);  /* 倒推 -> tick_pub */
    subscriber_subscribe(&bus, &s_log,  EVENT_ID_ANY);    /* 通配 -> 引擎链表 */

    /* 4. 发布 + 扫描：事件进所属引擎总队列，扫描时倒推发布者并分发回调 */
    printf("--- pass 1: post key ---\n");
    publisher_post(&key_pub, "A", 1);
    event_bus_process(&bus);

    printf("--- pass 2: post tick=1 ---\n");
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    event_bus_process(&bus);

    /* 连发两次再扫描：深度 1 只保留最新的 tick=3，tick=2 被覆盖 */
    printf("--- pass 3: post tick=2, tick=3 (only newest kept) ---\n");
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    event_bus_process(&bus);

    /* 5. 退订：s_log 用 EVENT_ID_ANY 退订（从本引擎所有链表全部摘除） */
    printf("--- pass 4: s_log unsubscribed, post key B ---\n");
    subscriber_unsubscribe(&bus, &s_log, EVENT_ID_ANY);
    publisher_post(&key_pub, "B", 1);
    event_bus_process(&bus);

    /* 6. 退订按键事件：按事件 ID 倒推发布者后摘链 */
    printf("--- pass 5: s_key unsubscribed, post key C ---\n");
    subscriber_unsubscribe(&bus, &s_key, EVENT_ID_KEY);
    publisher_post(&key_pub, "C", 1);
    event_bus_process(&bus);

    /* 7. 多实例演示：第二个引擎实例，独立队列/发布者表/通配链表，
     *    与实例 1 完全隔离（事件互不串扰） */
    {
        static event_bus_t  bus2;           /* 引擎实例 2（静态分配） */
        publisher_t  sensor_pub;
        subscriber_t s_sensor;

        printf("--- pass 6: second engine instance ---\n");
        event_bus_init(&bus2);
        publisher_init(&sensor_pub, EVENT_ID_SENSOR);
        event_bus_add_publisher(&bus2, &sensor_pub);

        subscriber_init(&s_sensor, on_sensor);
        subscriber_subscribe(&bus2, &s_sensor, EVENT_ID_SENSOR);

        publisher_post(&sensor_pub, "25C", 3);
        event_bus_process(&bus2);           /* 只扫描实例 2，实例 1 无输出 */

        event_bus_process(&bus);            /* 实例 1 队列为空，同样无输出 */
    }

    return 0;
}
