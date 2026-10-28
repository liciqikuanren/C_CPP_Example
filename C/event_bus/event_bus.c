#include "event_bus.h"
#include <string.h>

/* ==================== 事件总线引擎（全局唯一单例） ====================
 * 一个工程只有一个事件总线，结构体与实例（static g_bus）全部收在本文件内，
 * 所有内存静态分配；发布者与事件 ID 一一对应，ID 直接作查表数组下标。
 */
typedef struct event_bus {
    publisher_t  *pub_by_id[EVENT_ID_MAX];  /* 事件 ID -> 发布者 直接查表（ID 即下标，O(1)） */
    subscriber_t *any_subs;                 /* 通配（EVENT_ID_ANY）订阅者链表首地址 */

    event_t       queue[EVENT_QUEUE_LEN];   /* 全局唯一事件总队列 */
    uint8_t       head;                     /* 队头下标（出队方向） */
    uint8_t       tail;                     /* 队尾下标（入队方向） */
    uint8_t       count;                    /* 队列中事件个数 */
} event_bus_t;

static event_bus_t g_bus;                   /* 全局唯一事件总线实例 */

/* ==================== 链表内部工具 ==================== */

/**
 * 判断订阅者是否已挂在指定链表中（防止重复挂链形成环）
 */
static bool list_contains(const subscriber_t *head, const subscriber_t *sub)
{
    while (head) {
        if (head == sub) return true;
        head = head->next;
    }
    return false;
}

/**
 * 从指定链表中摘除订阅者
 * @param head 链表首地址的指针（首地址可能变化，故传二级指针）
 * @return true-找到并摘除  false-不在该链表中
 */
static bool list_unlink(subscriber_t **head, subscriber_t *sub)
{
    if (!head || !sub) return false;

    while (*head) {
        if (*head == sub) {
            *head = sub->next;              /* 前驱直接连后继，完成摘链 */
            sub->next = NULL;
            return true;
        }
        head = &(*head)->next;              /* 沿链表继续查找 */
    }
    return false;
}

/**
 * 把订阅者从所有链表摘除
 * （各发布者链表 + 总线通配链表），保证 next 指针唯一归属
 * 注意：调用前须已持有 EVENT_BUS_LOCK
 */
static void sub_unlink_all(subscriber_t *sub)
{
    uint8_t i;
    for (i = 0; i < EVENT_ID_MAX; i++) {
        if (g_bus.pub_by_id[i]) (void)list_unlink(&g_bus.pub_by_id[i]->subs, sub);
    }
    (void)list_unlink(&g_bus.any_subs, sub);
}

/**
 * 事件 ID 直接查表获取发布者实例
 * 发布者与事件 ID 一一对应，ID 即数组下标，O(1) 命中，无需遍历
 * （EVENT_ID_ANY 及越界 ID 返回 NULL）
 */
static publisher_t *find_publisher(event_id_t id)
{
    if ((unsigned)id >= (unsigned)EVENT_ID_MAX) return NULL;
    return g_bus.pub_by_id[id];
}

/**
 * 判断发布者是否已挂载到总线（查表命中且实例一致）
 */
static bool pub_registered(const publisher_t *pub)
{
    if (!pub) return false;
    if ((unsigned)pub->id >= (unsigned)EVENT_ID_MAX) return false;
    return g_bus.pub_by_id[pub->id] == pub;
}

/* ==================== 总队列（总线唯一队列） ==================== */

/**
 * 事件写入总线总队列
 * @return true-入队成功  false-队列已满（事件丢弃，保证发布非阻塞）
 */
static bool bus_enqueue(const event_t *ev)
{
    bool ok = false;

    EVENT_BUS_LOCK();
    if (g_bus.count < EVENT_QUEUE_LEN) {
        g_bus.queue[g_bus.tail] = *ev;      /* 存入队尾 */
        g_bus.tail = (uint8_t)((g_bus.tail + 1u) % EVENT_QUEUE_LEN);
        g_bus.count++;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 从总线总队列取出一个事件
 * @return true-取到事件  false-队列为空
 */
static bool bus_dequeue(event_t *out)
{
    bool ok = false;

    if (!out) return false;

    EVENT_BUS_LOCK();
    if (g_bus.count > 0u) {
        *out = g_bus.queue[g_bus.head];     /* 读取队头事件 */
        g_bus.head = (uint8_t)((g_bus.head + 1u) % EVENT_QUEUE_LEN);
        g_bus.count--;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/* ==================== 引擎接口 ==================== */

/**
 * 初始化事件引擎：清空总队列、ID 查表数组与通配链表
 */
void event_bus_init(void)
{
    EVENT_BUS_LOCK();
    memset(&g_bus, 0, sizeof(g_bus));
    EVENT_BUS_UNLOCK();
}

/**
 * 挂载发布者到总线：事件 ID 作下标直接写入查表数组，
 * 一一对应（同一 ID 不允许挂载两个不同发布者）
 * @return true-挂载成功（含重复挂载同一实例）  false-参数非法/ID 越界/ID 被占用
 */
bool event_bus_add_publisher(publisher_t *pub)
{
    bool ok = false;

    if (!pub) return false;
    if ((unsigned)pub->id >= (unsigned)EVENT_ID_MAX) return false;  /* 越界或 EVENT_ID_ANY */

    EVENT_BUS_LOCK();
    if ((g_bus.pub_by_id[pub->id] != NULL) && (g_bus.pub_by_id[pub->id] != pub)) {
        EVENT_BUS_UNLOCK();
        return false;                       /* 该 ID 已被其它发布者占用 */
    }
    g_bus.pub_by_id[pub->id] = pub;         /* ID 即下标，直接写表 */
    ok = true;
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 判断总线总队列是否为空
 */
bool event_bus_empty(void)
{
    return (g_bus.count == 0u);
}

/**
 * 引擎运行一轮处理：
 *   阶段 1：排空全局事件总队列，对每个事件用 pub_by_id[ev.id] 直接取出
 *           发布者（O(1)），沿其 subs 链表首地址遍历订阅者，把事件推入
 *           各订阅者缓存队列；同时推入通配链表中的 EVENT_ID_ANY 订阅者；
 *   阶段 2：遍历各发布者链表与通配链表，逐个出队订阅者缓存并调用回调。
 * 默认 SUB_QUEUE_LEN 为 1：入队后本扫描立即出队回调，
 * 每轮扫描即执行订阅者回调，仅缓存最新事件，内存占用最小。
 * 建议在主循环或任务中周期调用。
 */
void event_bus_process(void)
{
    uint8_t i;
    event_t ev;

    /* 阶段 1：总队列 -> 订阅者缓存队列 */
    while (bus_dequeue(&ev)) {
        publisher_t *pub = find_publisher(ev.id);    /* ID 直接查表取发布者 */
        subscriber_t *sub;

        if (pub) {                          /* 发布者链表 */
            sub = pub->subs;
            while (sub) {
                (void)subscriber_push(sub, &ev);    /* 满则覆盖旧事件，保留最新 */
                sub = sub->next;
            }
        }

        sub = g_bus.any_subs;               /* 通配链表（收所有事件） */
        while (sub) {
            (void)subscriber_push(sub, &ev);
            sub = sub->next;
        }
    }

    /* 阶段 2a：各发布者链表上的订阅者回调（按 ID 查表遍历非空槽位） */
    for (i = 0; i < EVENT_ID_MAX; i++) {
        subscriber_t *sub;
        if (!g_bus.pub_by_id[i]) continue;
        sub = g_bus.pub_by_id[i]->subs;
        while (sub) {
            event_t out;
            while (subscriber_pop(sub, &out)) {
                if (sub->handler) sub->handler(&out);
            }
            sub = sub->next;
        }
    }

    /* 阶段 2b：通配链表上的订阅者回调（只需处理一次，队列已汇总各事件） */
    {
        subscriber_t *sub = g_bus.any_subs;
        while (sub) {
            event_t out;
            while (subscriber_pop(sub, &out)) {
                if (sub->handler) sub->handler(&out);
            }
            sub = sub->next;
        }
    }
}

/* ==================== 订阅者实现 ==================== */

/**
 * 初始化订阅者：只绑定回调，清空缓存队列和链表指针
 * （此时尚未挂链，需调用 subscriber_subscribe 完成订阅）
 */
void subscriber_init(subscriber_t *sub, event_handler_t handler)
{
    if (!sub) return;
    memset(sub, 0, sizeof(*sub));
    sub->handler = handler;
}

/**
 * 订阅（订阅者视角的入口）：用事件 ID 直接查表取出发布者实例，
 * 再把本订阅者挂入该发布者的订阅者链表。
 * @param sub 订阅者
 * @param id  要订阅的事件枚举 ID；EVENT_ID_ANY 表示订阅所有事件
 * @return true-订阅成功  false-参数非法或未找到对应发布者
 */
bool subscriber_subscribe(subscriber_t *sub, event_id_t id)
{
    publisher_t *target = NULL;

    if (!sub) return false;

    EVENT_BUS_LOCK();

    /* 第 1 步：事件 ID 直接查表取发布者 */
    if (id != EVENT_ID_ANY) {
        target = find_publisher(id);
        if (!target) {                      /* 没有该事件的发布者 */
            EVENT_BUS_UNLOCK();
            return false;
        }
    }

    /* 第 2 步：先从原有链表全部摘除（一个订阅者只在一条链表中） */
    sub_unlink_all(sub);

    /* 第 3 步：头插挂入目标链表 */
    if (id == EVENT_ID_ANY) {
        sub->next   = g_bus.any_subs;       /* 通配：挂到总线通配链表首地址 */
        g_bus.any_subs = sub;
    } else {
        sub->next   = target->subs;         /* 头插：新节点指向原首地址 */
        target->subs = sub;                 /* 首地址更新为新节点 */
    }

    EVENT_BUS_UNLOCK();
    return true;
}

/**
 * 退订（订阅者视角的入口）：用事件 ID 直接查表取发布者并摘链
 * @param id 要退订的事件枚举 ID；EVENT_ID_ANY 表示从所有链表全部摘除
 */
void subscriber_unsubscribe(subscriber_t *sub, event_id_t id)
{
    publisher_t *target;

    if (!sub) return;

    EVENT_BUS_LOCK();

    if (id == EVENT_ID_ANY) {               /* 全部退订 */
        sub_unlink_all(sub);
        EVENT_BUS_UNLOCK();
        return;
    }

    target = find_publisher(id);            /* 事件 ID 直接查表 */
    if (target) (void)list_unlink(&target->subs, sub);

    EVENT_BUS_UNLOCK();
}

/**
 * 事件写入订阅者本地缓存队列（环形队列）
 * 队列满时覆盖最旧事件，始终保留最新事件
 * （默认深度为 1：本扫描入队后随即出队回调，等价于直接执行）
 * @return true-写入成功  false-参数非法
 */
bool subscriber_push(subscriber_t *sub, const event_t *ev)
{
    bool ok = false;
    if (!sub || !ev) return false;

    EVENT_BUS_LOCK();
    if (sub->count < SUB_QUEUE_LEN) {
        sub->queue[sub->tail] = *ev;        /* 存入队尾 */
        sub->tail = (uint8_t)((sub->tail + 1u) % SUB_QUEUE_LEN);
        sub->count++;
    } else {
        /* 队列满：覆盖队头最旧事件，队头/队尾同步后移（保留最新） */
        sub->queue[sub->tail] = *ev;
        sub->tail = (uint8_t)((sub->tail + 1u) % SUB_QUEUE_LEN);
        sub->head = (uint8_t)((sub->head + 1u) % SUB_QUEUE_LEN);
    }
    ok = true;
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 从订阅者缓存队列头部取出一个事件
 * @return true-取到事件  false-队列为空
 */
bool subscriber_pop(subscriber_t *sub, event_t *out)
{
    bool ok = false;
    if (!sub || !out) return false;

    EVENT_BUS_LOCK();
    if (sub->count > 0u) {
        *out = sub->queue[sub->head];       /* 读取队头事件 */
        sub->head = (uint8_t)((sub->head + 1u) % SUB_QUEUE_LEN);
        sub->count--;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 判断订阅者缓存队列是否为空
 */
bool subscriber_empty(const subscriber_t *sub)
{
    return (sub == NULL) || (sub->count == 0u);
}

/* ==================== 发布者实现 ==================== */

/**
 * 初始化发布者：绑定枚举 ID，清空订阅者链表
 * （发布者不自持事件队列，需挂载后才能发布）
 */
void publisher_init(publisher_t *pub, event_id_t id)
{
    if (!pub) return;
    memset(pub, 0, sizeof(*pub));
    pub->id   = id;
    pub->subs = NULL;                       /* 链表首地址置空 */
}

/**
 * 发布事件（发布者视角的入口）：只填发布者实例、数据、数据长度，
 * 事件 ID 自动取自发布者自身的枚举 ID，事件进入总线全局唯一总队列。
 * @param data 载荷数据（可为 NULL，表示空事件）
 * @param len  载荷长度，不得超过 EVENT_PAYLOAD_MAX
 * @return true-入队成功  false-未挂载/参数非法/超长/总队列已满（事件丢弃）
 */
bool publisher_post(publisher_t *pub, const void *data, uint16_t len)
{
    event_t ev;

    if (!pub) return false;
    if (!pub_registered(pub)) return false;         /* 未挂载到总线 */
    if (len > EVENT_PAYLOAD_MAX) return false;
    if (len && !data) return false;

    memset(&ev, 0, sizeof(ev));
    ev.id  = pub->id;                       /* 事件 ID 取发布者的枚举 ID */
    ev.len = len;
    if (len) memcpy(ev.payload, data, len);

    return bus_enqueue(&ev);                /* 统一进入总线总队列 */
}

/**
 * 底层挂链原语：把订阅者头插到发布者的订阅者链表
 * 头插法 O(1)，新的订阅者最先被遍历到
 * 注意：若该订阅者已挂在其它链表，会产生链表交叉，请优先使用
 *       subscriber_subscribe()
 */
void publisher_subscribe(publisher_t *pub, subscriber_t *sub)
{
    if (!pub || !sub) return;
    if (list_contains(pub->subs, sub)) return;  /* 已在链表中，直接返回 */

    EVENT_BUS_LOCK();
    sub->next = pub->subs;                  /* 新节点指向原首地址 */
    pub->subs = sub;                        /* 首地址更新为新节点 */
    EVENT_BUS_UNLOCK();
}

/**
 * 底层摘链原语：从发布者的订阅者链表中摘除该订阅者
 */
void publisher_unsubscribe(publisher_t *pub, subscriber_t *sub)
{
    if (!pub || !sub) return;

    EVENT_BUS_LOCK();
    (void)list_unlink(&pub->subs, sub);
    EVENT_BUS_UNLOCK();
}
