#include "txn_manager.h"
#include "txn_port.h"

#include <string.h>

/* 默认实例的静态存储：不带 _on 的老接口都在它上面操作 */
static txn_slot_t s_default_slots[TXN_MAX_SLOTS];
static txn_t s_default = { s_default_slots, TXN_MAX_SLOTS };

/* 判断是否超时（无符号减法，兼容计时器回绕） */
static int32_t expired(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

/* 组装最终结果并触发统一回调（成功/失败唯一出口） */
static void notify_call(const txn_req_cfg_t *cfg, txn_wait_ret_t wait_ret,
                        int request_ret, uint8_t retry,
                        const void *rx, size_t len)
{
    txn_result_t result;

    result.cmd_id = cfg->cmd_id;
    result.ctx = cfg->ctx;
    result.wait_ret = wait_ret;
    result.request_ret = request_ret;
    result.retry = retry;
    result.rx = rx;
    result.len = len;
    if (cfg->notify) {
        cfg->notify(&result);
    }
}

static int valid_instance(const txn_t *t)
{
    return t != NULL && t->slots != NULL && t->capacity > 0;
}

/* ==================================================================
 * 实例版 API
 * ================================================================== */

int txn_init_on(txn_t *t, txn_slot_t *slots, unsigned capacity)
{
    if (t == NULL || slots == NULL || capacity == 0) {
        return -1;
    }
    TXN_LOCK();
    memset(slots, 0, (size_t)capacity * sizeof(txn_slot_t));
    t->slots = slots;
    t->capacity = capacity;
    TXN_UNLOCK();
    return 0;
}

int txn_pending_count_on(const txn_t *t)
{
    int n = 0;

    if (!valid_instance(t)) {
        return 0;
    }
    TXN_LOCK();
    for (unsigned i = 0; i < t->capacity; i++) {
        if (t->slots[i].used) {
            n++;
        }
    }
    TXN_UNLOCK();
    return n;
}

int txn_submit_on(txn_t *t, const txn_req_cfg_t *cfg)
{
    if (!valid_instance(t) || cfg == NULL || cfg->request == NULL) {
        return -4;
    }

    int idx = -1;

    TXN_LOCK();
    /* 同一实例内 cmd_id 不允许并发等待，避免应答串包 */
    for (unsigned i = 0; i < t->capacity; i++) {
        if (t->slots[i].used && t->slots[i].cfg.cmd_id == cfg->cmd_id) {
            TXN_UNLOCK();
            return -2;
        }
    }
    /* 找空闲槽位 */
    for (unsigned i = 0; i < t->capacity; i++) {
        if (!t->slots[i].used) {
            idx = (int)i;
            break;
        }
    }
    if (idx < 0) {
        TXN_UNLOCK();
        return -1;
    }
    /* 占槽、拷贝配置、计算超时时刻 */
    t->slots[idx].used = 1;
    t->slots[idx].retry = 0;
    t->slots[idx].request_ret = 0;
    t->slots[idx].cfg = *cfg;
    t->slots[idx].deadline = txn_port_now_ms() + cfg->timeout_ms;
    TXN_UNLOCK();

    /* 槽位就绪后再发送；发送失败则释放槽位并回调 */
    int ret = cfg->request(cfg->ctx);
    if (ret != 0) {
        TXN_LOCK();
        t->slots[idx].used = 0;
        TXN_UNLOCK();
        notify_call(cfg, TXN_WAIT_TX_ERR, ret, 0, NULL, 0);
        return -3;
    }
    return idx;
}

int txn_on_rx_on(txn_t *t, const void *rx, size_t len)
{
    if (!valid_instance(t) || rx == NULL) {
        return 0;
    }

    TXN_LOCK();
    for (unsigned i = 0; i < t->capacity; i++) {
        txn_slot_t *slot = &t->slots[i];
        if (!slot->used) {
            continue;
        }
        /* 先拷贝所需信息，释放槽位后再调用回调（回调里可安全地链式提交） */
        txn_req_cfg_t cfg = slot->cfg;
        int request_ret = slot->request_ret;
        uint8_t retry = slot->retry;
        txn_match_t m = cfg.match ? cfg.match(rx, len, cfg.ctx) : TXN_MATCH_OK;
        if (m == TXN_MATCH_MORE) {
            continue; /* 不是这个事务的应答，看下一个槽位 */
        }
        slot->used = 0;
        TXN_UNLOCK();
        if (m == TXN_MATCH_OK) {
            notify_call(&cfg, TXN_WAIT_RESP_OK, request_ret, retry, rx, len);
        } else {
            notify_call(&cfg, TXN_WAIT_RESP_BAD, request_ret, retry, rx, len);
        }
        return 1;
    }
    TXN_UNLOCK();
    return 0;
}

void txn_poll_on(txn_t *t)
{
    if (!valid_instance(t)) {
        return;
    }

    uint32_t now = txn_port_now_ms();

    for (unsigned i = 0; i < t->capacity; i++) {
        txn_req_cfg_t cfg;
        uint8_t retry;
        int request_ret;
        int do_retry;

        TXN_LOCK();
        txn_slot_t *slot = &t->slots[i];
        if (!slot->used || !expired(now, slot->deadline)) {
            TXN_UNLOCK();
            continue;
        }
        cfg = slot->cfg;
        retry = slot->retry;
        request_ret = slot->request_ret;
        do_retry = (retry < cfg.max_retry);
        if (do_retry) {
            slot->retry++;
            slot->deadline = now + cfg.timeout_ms; /* 续上下一次等待 */
        } else {
            slot->used = 0; /* 重发次数用尽，释放槽位 */
        }
        TXN_UNLOCK();

        if (!do_retry) {
            /* 超时，等待最终返回值 = TIMEOUT */
            notify_call(&cfg, TXN_WAIT_TIMEOUT, request_ret, retry, NULL, 0);
            continue;
        }

        /* 超时重发；重发本身失败则立即判失败，回调带回 request() 的返回值 */
        int ret = cfg.request(cfg.ctx);
        if (ret != 0) {
            TXN_LOCK();
            t->slots[i].used = 0;
            TXN_UNLOCK();
            notify_call(&cfg, TXN_WAIT_TX_ERR, ret, (uint8_t)(retry + 1), NULL, 0);
        } else {
            TXN_LOCK();
            t->slots[i].request_ret = 0;
            TXN_UNLOCK();
        }
    }
}

int txn_abort_on(txn_t *t, int handle)
{
    if (!valid_instance(t) || handle < 0 || (unsigned)handle >= t->capacity) {
        return 0;
    }

    TXN_LOCK();
    txn_slot_t *slot = &t->slots[handle];
    if (!slot->used) {
        TXN_UNLOCK();
        return 0;
    }
    txn_req_cfg_t cfg = slot->cfg;
    int request_ret = slot->request_ret;
    uint8_t retry = slot->retry;
    slot->used = 0;
    TXN_UNLOCK();

    notify_call(&cfg, TXN_WAIT_CANCELLED, request_ret, retry, NULL, 0);
    return 1;
}

/* ==================================================================
 * 默认实例版 API：老接口，单管理器场景直接用
 * ================================================================== */

int txn_init(void)
{
    return txn_init_on(&s_default, s_default_slots, TXN_MAX_SLOTS);
}

int txn_pending_count(void)
{
    return txn_pending_count_on(&s_default);
}

int txn_submit(const txn_req_cfg_t *cfg)
{
    return txn_submit_on(&s_default, cfg);
}

int txn_on_rx(const void *rx, size_t len)
{
    return txn_on_rx_on(&s_default, rx, len);
}

void txn_poll(void)
{
    txn_poll_on(&s_default);
}

int txn_abort(int handle)
{
    return txn_abort_on(&s_default, handle);
}

/* 等待结果转字符串（便于打印日志） */
const char *txn_wait_ret_str(txn_wait_ret_t wait_ret)
{
    switch (wait_ret) {
    case TXN_WAIT_RESP_OK:
        return "RESP_OK";
    case TXN_WAIT_RESP_BAD:
        return "RESP_BAD";
    case TXN_WAIT_TIMEOUT:
        return "TIMEOUT";
    case TXN_WAIT_TX_ERR:
        return "TX_ERR";
    case TXN_WAIT_CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}
