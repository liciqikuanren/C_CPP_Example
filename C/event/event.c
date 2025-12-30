/* event.c
 * 纯软件版事件系统实现
 * 只依赖标准 C 库，可直接在 PC 上编译运行
 */

#include "event.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* 获取毫秒级时间戳（跨平台） */
static uint32_t get_time_ms(void)
{
    return (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
}

/* 调试打印 */
static void debug_print(const char* format, ...)
{
#if EVENT_DEBUG_ENABLE
    va_list args;
    va_start(args, format);
    printf("[EVENT] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
#endif
}

/* ==================== 事件队列 ==================== */
typedef struct {
    Event_t queue[EVENT_QUEUE_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} EventQueue_t;

static EventQueue_t g_queue;

/* 队列操作 */
static int queue_init(void)
{
    memset(&g_queue, 0, sizeof(g_queue));
    return 0;
}

static int queue_push(void* event_data)
{
    if (g_queue.count >= EVENT_QUEUE_SIZE) {
        return -1;  // 满
    }
    Event_t* e = (Event_t*)event_data;
    memcpy(&g_queue.queue[g_queue.tail], e, sizeof(Event_t));
    g_queue.tail = (g_queue.tail + 1) % EVENT_QUEUE_SIZE;
    g_queue.count++;
    return 0;
}

static void* queue_pop(void)
{
    if (g_queue.count == 0) {
        return NULL;
    }
    void* p = &g_queue.queue[g_queue.head];
    g_queue.head = (g_queue.head + 1) % EVENT_QUEUE_SIZE;
    g_queue.count--;
    return p;
}

static uint8_t queue_is_empty(void)
{
    return g_queue.count == 0;
}

static uint16_t queue_get_count(void)
{
    return g_queue.count;
}

/* ==================== 订阅者与观察者 ==================== */
typedef struct {
    EventCallback_t callback;
    void* arg;
    uint8_t used;
} Subscriber_t;

typedef struct {
    EventCallback_t callback;
    void* arg;
    uint8_t used;
} Observer_t;

static Subscriber_t g_subscribers[EVENT_MAX_COUNT][EVENT_SUBSCRIBER_MAX];
static Observer_t   g_observers[EVENT_OBSERVER_MAX];

static uint8_t g_initialized = 0;

/* ==================== 公共函数实现 ==================== */
int EVENT_Init(void)
{
    queue_init();
    memset(g_subscribers, 0, sizeof(g_subscribers));
    memset(g_observers, 0, sizeof(g_observers));
    g_initialized = 1;
    debug_print("Event system initialized");
    return 0;
}

int EVENT_Subscribe(Event_Type_t type, EventCallback_t callback, void* arg)
{
    if (!g_initialized || type >= EVENT_MAX_COUNT || callback == NULL) {
        return -1;
    }
    for (int i = 0; i < EVENT_SUBSCRIBER_MAX; i++) {
        if (!g_subscribers[type][i].used) {
            g_subscribers[type][i].callback = callback;
            g_subscribers[type][i].arg = arg;
            g_subscribers[type][i].used = 1;
            debug_print("Subscribed to event %u", type);
            return 0;
        }
    }
    return -1;  // 订阅者已满
}

int EVENT_Unsubscribe(Event_Type_t type, EventCallback_t callback, void* arg)
{
    if (!g_initialized || type >= EVENT_MAX_COUNT || callback == NULL) {
        return -1;
    }
    for (int i = 0; i < EVENT_SUBSCRIBER_MAX; i++) {
        if (g_subscribers[type][i].used &&
            g_subscribers[type][i].callback == callback &&
            g_subscribers[type][i].arg == arg) {
            g_subscribers[type][i].used = 0;
            debug_print("Unsubscribed from event %u", type);
            return 0;
        }
    }
    return -1;
}

int EVENT_Publish(Event_Type_t type, Event_Priority_t priority,
                  const void* data, uint8_t data_size)
{
    if (!g_initialized || type >= EVENT_MAX_COUNT) {
        return -1;
    }
    if (data && data_size > EVENT_DATA_SIZE_MAX) {
        return -1;
    }

    Event_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.priority = priority;
    event.timestamp = get_time_ms();
    if (data && data_size > 0) {
        event.data_size = data_size;
        memcpy(event.data, data, data_size);
    }

    if (queue_push(&event) != 0) {
        debug_print("Event %u dropped (queue full)", type);
        return -1;
    }

    debug_print("Event %u published", type);
    return 0;
}

static void dispatch_event(Event_t* event)
{
    /* 事件类型订阅者 */
    for (int i = 0; i < EVENT_SUBSCRIBER_MAX; i++) {
        if (g_subscribers[event->type][i].used) {
            g_subscribers[event->type][i].callback(event, g_subscribers[event->type][i].arg);
        }
    }
    /* 全局观察者 */
    for (int i = 0; i < EVENT_OBSERVER_MAX; i++) {
        if (g_observers[i].used) {
            g_observers[i].callback(event, g_observers[i].arg);
        }
    }
}

int EVENT_Process(void)
{
    if (!g_initialized) return 0;

    int count = 0;
    while (!queue_is_empty()) {
        Event_t* e = (Event_t*)queue_pop();
        if (e == NULL) break;
        dispatch_event(e);
        count++;
    }
    if (count > 0) {
        debug_print("Processed %d events", count);
    }
    return count;
}

int EVENT_ClearQueue(void)
{
    queue_init();
    debug_print("Event queue cleared");
    return 0;
}

uint16_t EVENT_GetCount(void)
{
    return queue_get_count();
}

int EVENT_RegisterObserver(EventCallback_t callback, void* arg)
{
    if (!g_initialized || callback == NULL) return -1;
    for (int i = 0; i < EVENT_OBSERVER_MAX; i++) {
        if (!g_observers[i].used) {
            g_observers[i].callback = callback;
            g_observers[i].arg = arg;
            g_observers[i].used = 1;
            debug_print("Observer registered");
            return 0;
        }
    }
    return -1;
}

int EVENT_UnregisterObserver(EventCallback_t callback)
{
    if (!g_initialized || callback == NULL) return -1;
    for (int i = 0; i < EVENT_OBSERVER_MAX; i++) {
        if (g_observers[i].used && g_observers[i].callback == callback) {
            g_observers[i].used = 0;
            debug_print("Observer unregistered");
            return 0;
        }
    }
    return -1;
}
