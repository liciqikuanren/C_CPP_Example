#ifndef TXN_MANAGER_H
#define TXN_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 槽位表大小（固定数组，编译期确定，可在编译选项中覆盖） */
#ifndef TXN_MAX_SLOTS
#define TXN_MAX_SLOTS 8u
#endif

/* 匹配结果：由 match 回调返回 */
typedef enum {
    TXN_MATCH_MORE = 0, /* 不是我的应答，继续看下一个槽位 */
    TXN_MATCH_OK,       /* 匹配成功，事务完成 */
    TXN_MATCH_ABORT     /* 是我的应答但校验失败，事务失败 */
} txn_match_t;

/* 等待应答的最终结果，写入回调的 wait_ret 字段 */
typedef enum {
    TXN_WAIT_RESP_OK = 0, /* 收到有效应答，事务成功 */
    TXN_WAIT_RESP_BAD,    /* 收到应答但 match 校验失败 */
    TXN_WAIT_TIMEOUT,     /* 超时且重发次数用尽 */
    TXN_WAIT_TX_ERR,      /* 发送请求失败，未进入等待 */
    TXN_WAIT_CANCELLED    /* 被 txn_abort() 取消 */
} txn_wait_ret_t;

/* 事务最终结果：成功与失败都通过同一个回调带回 */
typedef struct {
    uint16_t cmd_id;      /* 事务命令ID */
    void *ctx;            /* 提交时的用户上下文，用来区分是哪个事务 */
    txn_wait_ret_t wait_ret; /* 等待的最终返回值 */
    int request_ret;      /* 最近一次发送请求函数 request() 的返回值，0 = 发送成功 */
    uint8_t retry;        /* 实际发生过的重发次数 */
    const void *rx;       /* 应答数据，仅 RESP_OK / RESP_BAD 时有效，否则为 NULL */
    size_t len;           /* 应答数据长度 */
} txn_result_t;

/* 统一完成回调：成功和失败都走这里，用 wait_ret + ctx 区分 */
typedef void (*txn_notify_fn)(const txn_result_t *result);

/* 发送请求帧，返回 0 表示成功；返回值会带回给回调的 request_ret */
typedef int (*txn_request_fn)(void *ctx);
/* 校验收到的帧是否为本事务的应答 */
typedef txn_match_t (*txn_match_fn)(const void *rx, size_t len, void *ctx);

/* 事务请求配置：打包命令ID、函数指针、超时与重发参数 */
typedef struct {
    uint16_t cmd_id;        /* 关联ID，用于把应答配对回请求；等待中不允许重复 */
    txn_request_fn request; /* 发送请求 */
    txn_match_fn match;     /* 应答匹配校验，可为 NULL（任意帧都算应答） */
    txn_notify_fn notify;   /* 统一完成回调（成功/失败），可为 NULL */
    void *ctx;              /* 用户上下文，回调原样带回 */
    uint32_t timeout_ms;    /* 单次等待超时（毫秒） */
    uint8_t max_retry;      /* 超时后的重发次数，0 = 只发一次 */
} txn_req_cfg_t;

/* 初始化，清空整个槽位表 */
int txn_init(void);

/* 提交事务：占用空闲槽位并立即发送请求。
 * 返回 >=0 为事务句柄；-1 表满；-2 同 cmd_id 仍在等待；-3 发送失败；-4 参数非法。
 * 注意：-1/-2/-4 没有占用槽位，不会触发回调；-3 会触发回调(wait_ret=TX_ERR) */
int txn_submit(const txn_req_cfg_t *cfg);

/* 收到一帧数据时调用（主循环或中断均可），内部按 cmd_id 配对分发。
 * 匹配成功、校验失败都会触发统一回调。
 * 返回 1 = 已消费该帧，0 = 没有匹配的事务 */
int txn_on_rx(const void *rx, size_t len);

/* 主循环周期调用：处理超时与自动重发 */
void txn_poll(void);

/* 主动取消事务，会触发统一回调(wait_ret=CANCELLED)，返回 1 表示确实取消了一个事务 */
int txn_abort(int handle);

/* 当前等待中的事务数 */
int txn_pending_count(void);

/* 等待结果转字符串（便于打印日志） */
const char *txn_wait_ret_str(txn_wait_ret_t wait_ret);

#ifdef __cplusplus
}
#endif

#endif
