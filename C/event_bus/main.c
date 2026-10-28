/**
 * event_bus 使用示例
 * 演示：发布者（枚举 ID + 订阅者链表首地址，不自持队列，与事件 ID 一一对应）
 *       订阅者（缓存队列深度 1 + next 指针，按事件 ID 查表定位发布者完成订阅）
 *       引擎  （全局唯一单例，事件 ID 直接查表，扫描即回调）
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
    publisher_t  key_pub;                    /* 发布者：枚举 ID + 订阅者链表首地址 */
    publisher_t  tick_pub;
    subscriber_t s_key;                      /* 订阅者：深度1缓存队列 + next 指针 */
    subscriber_t s_tick;
    subscriber_t s_log;
    uint32_t     tick = 0;

    /* 1. 初始化引擎（单例），初始化发布者并挂载（ID 作下标直接写入查表数组） */
    event_bus_init();

    publisher_init(&key_pub,  EVENT_ID_KEY);
    publisher_init(&tick_pub, EVENT_ID_TIMER);

    event_bus_add_publisher(&key_pub);
    event_bus_add_publisher(&tick_pub);

    /* 2. 初始化订阅者：只绑定回调，不挂链 */
    subscriber_init(&s_key,  on_key);
    subscriber_init(&s_tick, on_timer);
    subscriber_init(&s_log,  on_log);

    /* 3. 订阅（订阅者发起）：传事件 ID，查表取发布者后挂入其链表 */
    subscriber_subscribe(&s_key,  EVENT_ID_KEY);    /* 查表 -> key_pub  */
    subscriber_subscribe(&s_tick, EVENT_ID_TIMER);  /* 查表 -> tick_pub */
    subscriber_subscribe(&s_log,  EVENT_ID_ANY);    /* 通配 -> 总线链表 */

    /* 4. 发布 + 扫描：事件统一进总线总队列，扫描时 ID 查表取发布者并分发回调 */
    printf("--- pass 1: post key ---\n");
    publisher_post(&key_pub, "A", 1);
    event_bus_process();

    printf("--- pass 2: post tick=1 ---\n");
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    event_bus_process();

    /* 连发两次再扫描：深度 1 只保留最新的 tick=3，tick=2 被覆盖 */
    printf("--- pass 3: post tick=2, tick=3 (only newest kept) ---\n");
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    tick++;
    publisher_post(&tick_pub, &tick, (uint16_t)sizeof(tick));
    event_bus_process();

    /* 5. 退订：s_log 用 EVENT_ID_ANY 退订（从所有链表全部摘除） */
    printf("--- pass 4: s_log unsubscribed, post key B ---\n");
    subscriber_unsubscribe(&s_log, EVENT_ID_ANY);
    publisher_post(&key_pub, "B", 1);
    event_bus_process();

    /* 6. 退订按键事件：查表取发布者后摘链 */
    printf("--- pass 5: s_key unsubscribed, post key C ---\n");
    subscriber_unsubscribe(&s_key, EVENT_ID_KEY);
    publisher_post(&key_pub, "C", 1);
    event_bus_process();

    return 0;
}
