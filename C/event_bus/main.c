/**
 * event_bus 使用示例
 * 演示：发布者（枚举 ID + 事件队列 + 订阅者链表首地址）
 *       订阅者（缓存队列 + next 指针，按事件 ID 倒推发布者完成订阅）
 *       引擎  （event_bus_process 驱动两级分发）
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

/* 万能日志回调：以 EVENT_ID_ANY 订阅，监听所有事件 */
static void on_log(const event_t *ev)
{
    printf("[sub_log ] id=%d len=%u\n", ev->id, ev->len);
}

int main(void)
{
    event_bus_t  bus;                        /* 事件总线处理引擎 */
    publisher_t  key_pub;                    /* 发布者：枚举 ID + 事件队列 + 链表首地址 */
    publisher_t  tick_pub;
    subscriber_t s_key;                      /* 订阅者：缓存队列 + next 指针 */
    subscriber_t s_tick;
    subscriber_t s_log;
    uint32_t     tick = 0;

    /* 1. 初始化引擎与发布者，并挂载到总线（挂载后才能被事件 ID 倒推查到） */
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
    subscriber_subscribe(&bus, &s_log,  EVENT_ID_ANY);    /* 通配 -> 总线链表 */

    /* 4. 发布事件：只填发布者实例 + 数据 + 长度，事件 ID 由发布者自动填入 */
    publisher_post(&key_pub, "A", 1);
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));

    /* 5. 引擎运行：事件分发 + 回调执行 */
    printf("--- process pass 1 ---\n");
    event_bus_process(&bus);

    /* 队列已空，重复调用无输出 */
    printf("--- process pass 2 (empty) ---\n");
    event_bus_process(&bus);

    /* 6. 退订：s_log 用 EVENT_ID_ANY 退订（从所有链表全部摘除） */
    subscriber_unsubscribe(&bus, &s_log, EVENT_ID_ANY);
    publisher_post(&key_pub, "B", 1);

    printf("--- process pass 3 (s_log unsubscribed) ---\n");
    event_bus_process(&bus);

    /* 7. 退订按键事件：按事件 ID 倒推发布者后摘链 */
    subscriber_unsubscribe(&bus, &s_key, EVENT_ID_KEY);
    publisher_post(&key_pub, "C", 1);

    printf("--- process pass 4 (s_key unsubscribed) ---\n");
    event_bus_process(&bus);

    return 0;
}
