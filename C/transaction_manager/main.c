/*
 * 事务管理器演示（模拟主机-从机请求应答）
 *
 * 流程：
 *   1. 应用层 txn_submit_on() 提交配置（命令ID + 发送函数 + 匹配函数 + 统一回调 + 超时重发）
 *   2. 主循环里周期调 txn_poll_on() 处理超时与自动重发
 *   3. 收到从机应答时调 txn_on_rx_on()，按命令ID自动配对
 *   4. 无论成功失败，都走同一个回调 notify：
 *      用 result->ctx 区分是哪个事务，用 result->wait_ret 看等待的最终结果，
 *      用 result->request_ret 看发送请求函数的返回值
 *
 * 本 demo 用实例版 API（组件化用法：一张自己的槽位表 s_bus），
 * 场景4 演示不带 _on 的默认实例兼容写法。
 *
 * 场景：
 *   场景1：从机应答慢，但没超时 → 成功
 *   场景2：两个命令并发，应答乱序到达 → 依然配对正确
 *   场景3：从机不应答 → 自动重发 2 次后超时失败
 *   场景4：默认实例（老接口）用法兼容
 */
#include "txn_manager.h"
#include "txn_port.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static uint32_t s_now; /* 模拟毫秒节拍 */
static int s_fail;

/* 实例版用法：本 demo 模拟一条总线，自带一张槽位表（组件化形态） */
static txn_slot_t s_bus_slots[TXN_MAX_SLOTS];
static txn_t s_bus;

/* 源文件是 UTF-8，Windows 控制台默认 GBK(936) 会显示乱码，
 * 这里把控制台输出代码页切到 UTF-8 */
static void console_utf8(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

uint32_t txn_port_now_ms(void)
{
    return s_now;
}

#define CHECK(cond, msg)                         \
    do {                                         \
        if (cond) {                              \
            printf("PASS: %s\n", (msg));         \
        } else {                                 \
            printf("FAIL: %s\n", (msg));         \
            s_fail++;                            \
        }                                        \
    } while (0)

/* ---- 模拟从机 -------------------------------------------------- */

typedef struct {
    uint16_t cmd_id;
    int silent;        /* 1 = 不应答 */
    uint32_t latency;  /* 应答延迟毫秒 */
    int send_count;    /* 被请求次数 */
    int done;          /* 1 = 收到有效应答 */
    int wait_ret;      /* 回调带回：等待的最终返回值 */
    int request_ret;   /* 回调带回：发送请求函数的返回值 */
    int retry;         /* 回调带回：实际重发次数 */
} mock_dev_t;

typedef struct {
    uint32_t arrive; /* 应答到达时刻 */
    uint8_t buf[3];  /* 帧：cmd_id 高字节 + 低字节 + 校验 */
    int used;
} mock_resp_t;

static mock_resp_t s_resp[4];
static uint16_t s_done_order[4]; /* 记录完成顺序，用于验证乱序场景 */
static int s_done_num;

/* 简单校验：高字节 ^ 低字节 ^ 0xA5 */
static uint8_t frame_crc(uint16_t id)
{
    return (uint8_t)((uint8_t)(id >> 8) ^ (uint8_t)id ^ 0xA5u);
}

/* 发送请求：把一帧"应答"排入队列，到时间后由主循环投递 */
static int mock_request(void *ctx)
{
    mock_dev_t *dev = (mock_dev_t *)ctx;
    dev->send_count++;
    if (dev->silent) {
        return 0;
    }
    for (unsigned i = 0; i < 4u; i++) {
        if (!s_resp[i].used) {
            s_resp[i].used = 1;
            s_resp[i].arrive = s_now + dev->latency;
            s_resp[i].buf[0] = (uint8_t)(dev->cmd_id >> 8);
            s_resp[i].buf[1] = (uint8_t)dev->cmd_id;
            s_resp[i].buf[2] = frame_crc(dev->cmd_id);
            return 0;
        }
    }
    return -1;
}

/* 应答匹配：ID 不对返回 MORE（继续等），校验错返回 ABORT */
static txn_match_t mock_match(const void *rx, size_t len, void *ctx)
{
    const mock_dev_t *dev = (const mock_dev_t *)ctx;
    const uint8_t *b = (const uint8_t *)rx;
    uint16_t id;

    if (len < 3) {
        return TXN_MATCH_MORE;
    }
    id = (uint16_t)((b[0] << 8) | b[1]);
    if (id != dev->cmd_id) {
        return TXN_MATCH_MORE;
    }
    if (b[2] != frame_crc(id)) {
        return TXN_MATCH_ABORT;
    }
    return TXN_MATCH_OK;
}

/* 统一回调：成功和失败都进这里，靠 ctx 找到是哪个事务 */
static void mock_notify(const txn_result_t *result)
{
    mock_dev_t *dev = (mock_dev_t *)result->ctx;

    dev->wait_ret = (int)result->wait_ret;
    dev->request_ret = result->request_ret;
    dev->retry = result->retry;
    printf("   回调: cmd=0x%04X wait=%s request_ret=%d retry=%u\n",
           result->cmd_id, txn_wait_ret_str(result->wait_ret),
           result->request_ret, result->retry);

    if (result->wait_ret == TXN_WAIT_RESP_OK) {
        dev->done = 1;
        if (s_done_num < (int)(sizeof(s_done_order) / sizeof(s_done_order[0]))) {
            s_done_order[s_done_num++] = dev->cmd_id;
        }
    }
}

/* 组装配置并提交到指定实例，返回事务句柄 */
static int submit_dev(txn_t *t, mock_dev_t *dev, uint32_t timeout_ms, uint8_t max_retry)
{
    txn_req_cfg_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.cmd_id = dev->cmd_id;
    cfg.request = mock_request;
    cfg.match = mock_match;
    cfg.notify = mock_notify;
    cfg.ctx = dev;
    cfg.timeout_ms = timeout_ms;
    cfg.max_retry = max_retry;
    return txn_submit_on(t, &cfg);
}

/* ---- 模拟主循环 ------------------------------------------------ */

/* 投递已到时间的从机应答 */
static void deliver_due(txn_t *t)
{
    for (unsigned i = 0; i < 4u; i++) {
        if (s_resp[i].used && (int32_t)(s_now - s_resp[i].arrive) >= 0) {
            txn_on_rx_on(t, s_resp[i].buf, 3);
            s_resp[i].used = 0;
        }
    }
}

/* 推进虚拟时间，相当于主循环跑 ms 毫秒 */
static void sim_run(txn_t *t, uint32_t ms)
{
    uint32_t end = s_now + ms;
    while ((int32_t)(s_now - end) < 0) {
        s_now += 5;
        deliver_due(t);
        txn_poll_on(t);
    }
}

/* ---- 测试场景 -------------------------------------------------- */

int main(void)
{
    console_utf8();
    txn_init_on(&s_bus, s_bus_slots, TXN_MAX_SLOTS);

    printf("== 场景1：应答慢但未超时 ==\n");
    {
        mock_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.cmd_id = 0x0001;
        dev.latency = 55; /* 应答要 55ms，超时设 100ms */
        dev.wait_ret = -1;

        CHECK(submit_dev(&s_bus, &dev, 100, 0) >= 0, "提交成功");
        sim_run(&s_bus, 120);
        CHECK(dev.done, "收到应答，事务完成");
        CHECK(dev.wait_ret == TXN_WAIT_RESP_OK, "回调 wait_ret = RESP_OK");
        CHECK(dev.request_ret == 0, "回调 request_ret = 0（发送成功）");
        CHECK(dev.send_count == 1, "只发送了一次");
        CHECK(txn_pending_count_on(&s_bus) == 0, "槽位已释放");
    }

    printf("== 场景2：两个命令并发，应答乱序 ==\n");
    {
        mock_dev_t a, b;
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        a.cmd_id = 0x0010;
        a.latency = 40;
        b.cmd_id = 0x0011;
        b.latency = 10; /* 后发的先回 */
        a.wait_ret = -1;
        b.wait_ret = -1;
        s_done_num = 0;

        CHECK(submit_dev(&s_bus, &a, 100, 0) >= 0, "命令 0x10 提交成功");
        CHECK(submit_dev(&s_bus, &b, 100, 0) >= 0, "命令 0x11 提交成功");
        CHECK(txn_pending_count_on(&s_bus) == 2, "两个事务同时等待中");
        sim_run(&s_bus, 120);
        CHECK(a.done && b.done, "两个事务都完成");
        CHECK(s_done_num == 2 && s_done_order[0] == 0x0011 && s_done_order[1] == 0x0010,
              "乱序到达仍按命令ID配对正确");
    }

    printf("== 场景3：从机不应答，超时重发 ==\n");
    {
        mock_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.cmd_id = 0x0002;
        dev.silent = 1; /* 永不应答 */
        dev.wait_ret = -1;

        CHECK(submit_dev(&s_bus, &dev, 20, 2) >= 0, "提交成功");
        sim_run(&s_bus, 100);
        CHECK(dev.wait_ret == TXN_WAIT_TIMEOUT, "回调 wait_ret = TIMEOUT");
        CHECK(dev.request_ret == 0, "发送本身成功，只是没人应答");
        CHECK(dev.retry == 2, "回调带回重发次数 2");
        CHECK(dev.send_count == 3, "首发 + 重发2次 = 共3次");
        CHECK(!dev.done, "事务未完成");
        CHECK(txn_pending_count_on(&s_bus) == 0, "槽位已释放");
    }

    printf("== 场景4：默认实例（不带 _on 的老接口） ==\n");
    {
        mock_dev_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.cmd_id = 0x0003;
        dev.silent = 1;
        dev.wait_ret = -1;

        /* 下面这组是默认实例 API，单管理器场景可直接这么写 */
        txn_init();
        txn_req_cfg_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.cmd_id = dev.cmd_id;
        cfg.request = mock_request;
        cfg.match = mock_match;
        cfg.notify = mock_notify;
        cfg.ctx = &dev;
        cfg.timeout_ms = 500;

        int h = txn_submit(&cfg);
        CHECK(h >= 0, "默认实例提交成功");
        CHECK(txn_pending_count() == 1, "默认实例表与 s_bus 相互独立");
        CHECK(txn_pending_count_on(&s_bus) == 0, "s_bus 不受默认实例影响");
        CHECK(txn_abort(h) == 1, "默认实例取消成功");
        CHECK(dev.wait_ret == TXN_WAIT_CANCELLED, "回调 wait_ret = CANCELLED");
        CHECK(txn_pending_count() == 0, "默认实例槽位已释放");
    }

    printf("\n%s（%d 个失败）\n", s_fail ? "结果：FAILED" : "结果：ALL PASSED", s_fail);
    return s_fail ? 1 : 0;
}
