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
 * 把订阅者从总线相关的所有链表摘除
 * （各发布者链表 + 总线通配链表），保证 next 指针唯一归属
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
 * 从发布者事件队列中取出一个事件（内部函数）
 * @param pub 发布者
 * @param out 输出事件
 * @return true-取到事件  false-队列为空
 */
static bool publisher_pop_one(publisher_t *pub, event_t *out)
{
    bool ok = false;
    if (!pub || !out) return false;

    EVENT_BUS_LOCK();                       /* 进入临界区 */
    if (pub->count > 0u) {
        *out = pub->queue[pub->head];       /* 读取队头事件 */
        pub->head = (uint8_t)((pub->head + 1u) % PUB_QUEUE_LEN);  /* 环形后移 */
        pub->count--;
        ok = true;
    }
    EVENT_BUS_UNLOCK();                     /* 退出临界区 */
    return ok;
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
 * 订阅（订阅者视角的入口）：用事件 ID 倒推是哪个发布者，
 * 再把本订阅者挂入该发布者的订阅者链表。
 * @param bus 事件总线（提供发布者表，用于事件 ID 倒推发布者）
 * @param sub 订阅者
 * @param id  要订阅的事件枚举 ID；EVENT_ID_ANY 表示订阅所有事件
 * @return true-订阅成功  false-参数非法或未找到对应发布者
 */
bool subscriber_subscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)
{
    publisher_t *target = NULL;
    uint8_t i;

    if (!bus || !sub) return false;

    EVENT_BUS_LOCK();

    /* 第 1 步：事件 ID 倒推发布者 */
    if (id != EVENT_ID_ANY) {
        for (i = 0; i < bus->pub_cnt; i++) {
            if (bus->pubs[i] && bus->pubs[i]->id == id) {
                target = bus->pubs[i];
                break;
            }
        }
        if (!target) {                      /* 总线上没有该事件的发布者 */
            EVENT_BUS_UNLOCK();
            return false;
        }
    }

    /* 第 2 步：先从原有链表全部摘除（一个订阅者只在一条链表中） */
    sub_unlink_all(bus, sub);

    /* 第 3 步：头插挂入目标链表 */
    if (id == EVENT_ID_ANY) {
        sub->next  = bus->any_subs;         /* 通配：挂到总线链表首地址 */
        bus->any_subs = sub;
    } else {
        sub->next  = target->subs;          /* 头插：新节点指向原首地址 */
        target->subs = sub;                 /* 首地址更新为新节点 */
    }

    EVENT_BUS_UNLOCK();
    return true;
}

/**
 * 退订（订阅者视角的入口）：用事件 ID 倒推发布者，从其链表摘除
 * @param id 要退订的事件枚举 ID；EVENT_ID_ANY 表示从所有链表全部摘除
 */
void subscriber_unsubscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)
{
    publisher_t *target = NULL;
    uint8_t i;

    if (!bus || !sub) return;

    EVENT_BUS_LOCK();

    if (id == EVENT_ID_ANY) {               /* 全部退订 */
        sub_unlink_all(bus, sub);
        EVENT_BUS_UNLOCK();
        return;
    }

    /* 事件 ID 倒推发布者，再从该发布者链表摘除 */
    for (i = 0; i < bus->pub_cnt; i++) {
        if (bus->pubs[i] && bus->pubs[i]->id == id) {
            target = bus->pubs[i];
            break;
        }
    }
    if (target) (void)list_unlink(&target->subs, sub);

    EVENT_BUS_UNLOCK();
}

/**
 * 事件写入订阅者本地缓存队列（环形队列）
 * @return true-写入成功  false-参数非法或队列已满（事件丢弃）
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
        ok = true;
    }
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
 * 初始化发布者：绑定枚举 ID，清空事件队列与订阅者链表
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
 * 事件 ID 自动取自发布者自身的枚举 ID，无需调用方填写。
 * @param data 载荷数据（可为 NULL，表示空事件）
 * @param len  载荷长度，不得超过 EVENT_PAYLOAD_MAX
 * @return true-入队成功  false-参数非法、超长或队列已满（事件丢弃）
 */
bool publisher_post(publisher_t *pub, const void *data, uint16_t len)
{
    event_t ev;
    bool ok = false;

    if (!pub) return false;
    if (len > EVENT_PAYLOAD_MAX) return false;
    if (len && !data) return false;

    memset(&ev, 0, sizeof(ev));
    ev.id  = pub->id;                       /* 事件 ID 取发布者的枚举 ID */
    ev.len = len;
    if (len) memcpy(ev.payload, data, len);

    EVENT_BUS_LOCK();
    if (pub->count < PUB_QUEUE_LEN) {
        pub->queue[pub->tail] = ev;         /* 存入队尾 */
        pub->tail = (uint8_t)((pub->tail + 1u) % PUB_QUEUE_LEN);
        pub->count++;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
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

/**
 * 判断发布者事件队列是否为空
 */
bool publisher_empty(const publisher_t *pub)
{
    return (pub == NULL) || (pub->count == 0u);
}

/* ==================== 事件总线处理引擎实现 ==================== */

/**
 * 初始化事件总线引擎，清空发布者表与通配链表
 */
void event_bus_init(event_bus_t *bus)
{
    if (!bus) return;
    memset(bus, 0, sizeof(*bus));
}

/**
 * 挂载发布者到引擎（只有挂载后才能被事件 ID 倒推查到）
 * @return true-挂载成功  false-参数非法或发布者表已满
 */
bool event_bus_add_publisher(event_bus_t *bus, publisher_t *pub)
{
    bool ok = false;
    if (!bus || !pub) return false;

    EVENT_BUS_LOCK();
    if (bus->pub_cnt < EVENT_BUS_MAX_PUBS) {
        bus->pubs[bus->pub_cnt++] = pub;
        ok = true;
    }
    EVENT_BUS_UNLOCK();
    return ok;
}

/**
 * 引擎运行一轮处理：
 *   阶段 1：排空每个发布者的事件队列，从 subs 链表首地址开始遍历订阅者，
 *           把事件推入各订阅者的本地缓存队列；同时推入总线通配链表
 *           中以 EVENT_ID_ANY 订阅的订阅者；
 *   阶段 2：再次遍历链表，逐个出队订阅者缓存事件并调用其回调，
 *           最后处理通配链表。
 * 事件能到达哪个订阅者，由链表挂接关系决定——订阅者订阅了哪个事件 ID，
 * 就被倒推挂到哪个发布者的链表上。
 * 建议在主循环或任务中周期调用。
 */
void event_bus_process(event_bus_t *bus)
{
    uint8_t i;

    if (!bus) return;

    for (i = 0; i < bus->pub_cnt; i++) {
        publisher_t *pub = bus->pubs[i];
        event_t ev;

        if (!pub) continue;

        /* 阶段 1：发布者队列 -> 订阅者缓存队列 */
        while (publisher_pop_one(pub, &ev)) {
            subscriber_t *sub = pub->subs;  /* 从链表首地址开始遍历 */
            while (sub) {
                (void)subscriber_push(sub, &ev);  /* 队列满则丢弃 */
                sub = sub->next;            /* 访问下一个订阅者 */
            }

            /* 通配订阅者（EVENT_ID_ANY）接收所有发布者的事件 */
            sub = bus->any_subs;
            while (sub) {
                (void)subscriber_push(sub, &ev);
                sub = sub->next;
            }
        }

        /* 阶段 2a：本发布者链表上的订阅者回调 */
        {
            subscriber_t *sub = pub->subs;  /* 重新从链表首地址开始 */
            while (sub) {
                event_t out;
                while (subscriber_pop(sub, &out)) {
                    if (sub->handler) sub->handler(&out);
                }
                sub = sub->next;
            }
        }
    }

    /* 阶段 2b：通配链表上的订阅者回调（只需处理一次，队列已汇总各发布者事件） */
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
