#include "event_bus.h"
#include <string.h>

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
 * 把订阅者从指定引擎的所有链表摘除
 * （各发布者链表 + 引擎通配链表），保证 next 指针唯一归属
 * 注意：调用前须已持有 EVENT_BUS_LOCK
 */
static void sub_unlink_all(event_bus_t *bus, subscriber_t *sub)
{
    uint8_t i;
    for (i = 0; i < bus->pub_cnt; i++) {
        if (bus->pubs[i]) (void)list_unlink(&bus->pubs[i]->subs, sub);
    }
    (void)list_unlink(&bus->any_subs, sub);
}

/**
 * 事件 ID 倒推发布者：在指定引擎的发布者表中查找 id 匹配的发布者
 * 注意：一个引擎上，一个事件 ID 只应挂载一个发布者
 */
static publisher_t *find_publisher(event_bus_t *bus, event_id_t id)
{
    uint8_t i;
    for (i = 0; i < bus->pub_cnt; i++) {
        if (bus->pubs[i] && bus->pubs[i]->id == id) return bus->pubs[i];
    }
    return NULL;
}

/* ==================== 引擎总队列 ==================== */

/**
 * 事件写入指定引擎的总队列
 * @return true-入队成功  false-队列已满（事件丢弃，保证发布非阻塞）
 */
static bool bus_enqueue(event_bus_t *bus, const event_t *ev)
{
    bool ok = false;

    EVENT_BUS_LOCK();
    if (bus->count < EVENT_QUEUE_LEN) {
        bus->queue[bus->tail] = *ev;        /* 存入队尾 */
        bus->tail = (uint8_t)((bus->tail + 1u) % EVENT_QUEUE_LEN);
        bus->count++;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 从指定引擎的总队列取出一个事件
 * @return true-取到事件  false-队列为空
 */
static bool bus_dequeue(event_bus_t *bus, event_t *out)
{
    bool ok = false;

    if (!out) return false;

    EVENT_BUS_LOCK();
    if (bus->count > 0u) {
        *out = bus->queue[bus->head];       /* 读取队头事件 */
        bus->head = (uint8_t)((bus->head + 1u) % EVENT_QUEUE_LEN);
        bus->count--;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/* ==================== 引擎接口 ==================== */

/**
 * 初始化事件引擎：清空总队列、发布者表与通配链表
 */
void event_bus_init(event_bus_t *bus)
{
    if (!bus) return;

    EVENT_BUS_LOCK();
    memset(bus, 0, sizeof(*bus));
    EVENT_BUS_UNLOCK();
}

/**
 * 挂载发布者到引擎（挂载后才能被事件 ID 倒推查到），
 * 并记录发布者归属，供 publisher_post 投递到本引擎总队列
 * @return true-挂载成功  false-参数非法或发布者表已满
 */
bool event_bus_add_publisher(event_bus_t *bus, publisher_t *pub)
{
    bool ok = false;
    if (!bus || !pub) return false;

    EVENT_BUS_LOCK();
    if (bus->pub_cnt < EVENT_BUS_MAX_PUBS) {
        bus->pubs[bus->pub_cnt++] = pub;
        pub->bus = bus;                     /* 记录所属引擎 */
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 判断指定引擎的总队列是否为空
 */
bool event_bus_empty(const event_bus_t *bus)
{
    return (bus == NULL) || (bus->count == 0u);
}

/**
 * 引擎运行一轮处理：
 *   阶段 1：排空本引擎的事件总队列，对每个事件用其 ID 倒推发布者，
 *           沿该发布者的 subs 链表首地址遍历订阅者，把事件推入各订阅者
 *           缓存队列；同时推入通配链表中的 EVENT_ID_ANY 订阅者；
 *   阶段 2：遍历各发布者链表与通配链表，逐个出队订阅者缓存并调用回调。
 * 默认 SUB_QUEUE_LEN 为 1：入队后本扫描立即出队回调，
 * 每轮扫描即执行订阅者回调，仅缓存最新事件，内存占用最小。
 * 建议在主循环或任务中周期调用。
 */
void event_bus_process(event_bus_t *bus)
{
    uint8_t i;
    event_t ev;

    if (!bus) return;

    /* 阶段 1：总队列 -> 订阅者缓存队列 */
    while (bus_dequeue(bus, &ev)) {
        publisher_t *pub = find_publisher(bus, ev.id); /* 事件 ID 倒推发布者 */
        subscriber_t *sub;

        if (pub) {                          /* 发布者链表 */
            sub = pub->subs;
            while (sub) {
                (void)subscriber_push(sub, &ev);    /* 满则覆盖旧事件，保留最新 */
                sub = sub->next;
            }
        }

        sub = bus->any_subs;                /* 通配链表（收所有事件） */
        while (sub) {
            (void)subscriber_push(sub, &ev);
            sub = sub->next;
        }
    }

    /* 阶段 2a：各发布者链表上的订阅者回调 */
    for (i = 0; i < bus->pub_cnt; i++) {
        subscriber_t *sub;
        if (!bus->pubs[i]) continue;
        sub = bus->pubs[i]->subs;
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
        subscriber_t *sub = bus->any_subs;
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
 * 订阅（订阅者视角的入口）：在指定引擎上用事件 ID 倒推是哪个发布者，
 * 再把本订阅者挂入该发布者的订阅者链表。
 * @param bus 目标引擎
 * @param sub 订阅者
 * @param id  要订阅的事件枚举 ID；EVENT_ID_ANY 表示订阅该引擎的所有事件
 * @return true-订阅成功  false-参数非法或未找到对应发布者
 */
bool subscriber_subscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)
{
    publisher_t *target = NULL;

    if (!bus || !sub) return false;

    EVENT_BUS_LOCK();

    /* 第 1 步：事件 ID 倒推发布者 */
    if (id != EVENT_ID_ANY) {
        target = find_publisher(bus, id);
        if (!target) {                      /* 该引擎没有此事件的发布者 */
            EVENT_BUS_UNLOCK();
            return false;
        }
    }

    /* 第 2 步：先从原有链表全部摘除（一个订阅者只在一条链表中） */
    sub_unlink_all(bus, sub);

    /* 第 3 步：头插挂入目标链表 */
    if (id == EVENT_ID_ANY) {
        sub->next   = bus->any_subs;        /* 通配：挂到引擎通配链表首地址 */
        bus->any_subs = sub;
    } else {
        sub->next   = target->subs;         /* 头插：新节点指向原首地址 */
        target->subs = sub;                 /* 首地址更新为新节点 */
    }

    EVENT_BUS_UNLOCK();
    return true;
}

/**
 * 退订（订阅者视角的入口）：在指定引擎上用事件 ID 倒推发布者并摘链
 * @param bus 目标引擎
 * @param id  要退订的事件枚举 ID；EVENT_ID_ANY 表示从该引擎所有链表全部摘除
 */
void subscriber_unsubscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)
{
    publisher_t *target;

    if (!bus || !sub) return;

    EVENT_BUS_LOCK();

    if (id == EVENT_ID_ANY) {               /* 全部退订 */
        sub_unlink_all(bus, sub);
        EVENT_BUS_UNLOCK();
        return;
    }

    target = find_publisher(bus, id);       /* 事件 ID 倒推发布者 */
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
 * 初始化发布者：绑定枚举 ID，清空订阅者链表与归属引擎指针
 * （发布者不再自持事件队列，需挂载后才能发布）
 */
void publisher_init(publisher_t *pub, event_id_t id)
{
    if (!pub) return;
    memset(pub, 0, sizeof(*pub));
    pub->id   = id;
    pub->subs = NULL;                       /* 链表首地址置空 */
    pub->bus  = NULL;                       /* 挂载后由 event_bus_add_publisher 写入 */
}

/**
 * 发布事件（发布者视角的入口）：只填发布者实例、数据、数据长度，
 * 事件 ID 自动取自发布者自身的枚举 ID，
 * 事件投递到发布者所属引擎的总队列（挂载时记录归属，无需显式传引擎）。
 * @param data 载荷数据（可为 NULL，表示空事件）
 * @param len  载荷长度，不得超过 EVENT_PAYLOAD_MAX
 * @return true-入队成功  false-未挂载/参数非法/超长/总队列已满（事件丢弃）
 */
bool publisher_post(publisher_t *pub, const void *data, uint16_t len)
{
    event_t ev;

    if (!pub || !pub->bus) return false;    /* 未挂载到任何引擎 */
    if (len > EVENT_PAYLOAD_MAX) return false;
    if (len && !data) return false;

    memset(&ev, 0, sizeof(ev));
    ev.id  = pub->id;                       /* 事件 ID 取发布者的枚举 ID */
    ev.len = len;
    if (len) memcpy(ev.payload, data, len);

    return bus_enqueue(pub->bus, &ev);      /* 投递到所属引擎的总队列 */
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
