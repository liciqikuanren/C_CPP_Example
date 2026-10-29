# 事务管理器（Transaction Manager）

主机给从机发命令、等应答是嵌入式里最常见的通信模式：应答可能很快，也可能几十毫秒甚至更久才回来。直接阻塞等待会拖死主循环，不等待又要把"发出去了没有、回没回来、超没超时、要不要重发"这些状态散落在应用层各处，难维护。

事务管理器就是把这些杂活收拢起来：**应用层只需要提交一个配置（命令ID + 发送函数 + 匹配函数 + 回调），然后在主循环里调一个 `txn_poll()`，应答配对、超时、自动重发全部由管理器完成。**

## 特性

- 固定大小槽位表（编译期确定 `TXN_MAX_SLOTS`），**无 malloc、无内存碎片**，适合裸机
- 按**命令ID/序列号自动配对**应答，支持多事务并发、应答乱序到达
- 每个事务独立的**超时 + 可配置自动重发**
- **统一回调**：成功/失败走同一个回调，回调参数带回上下文、发送函数返回值、等待最终结果
- **非阻塞**：纯主循环轮询模型，无 RTOS 依赖，标准 C11
- 回调内可安全地**链式提交**下一个事务（先释放槽位再回调）
- 时间用无符号减法比较，兼容系统 tick 计数器回绕
- 临界区 `TXN_LOCK/UNLOCK` 可配置，默认空实现（单线程裸机够用）

## 目录结构

```
transaction_manager/
├── CMakeLists.txt
├── main.c                 # 演示程序（模拟主机-从机，3 个场景）
├── inc/
│   ├── txn_manager.h      # 管理器接口
│   └── txn_port.h         # 移植层：时间/临界区
└── src/
    └── txn_manager.c      # 管理器实现
```

## 工作原理

```
应用层                     事务管理器                        通信对端
  │                                                    │
  │ txn_submit(cfg) ──── 占空闲槽位,立即调 request() ──→│ 发出请求
  │                        ▲                            │
  │                        │ 槽位表[N]: 等待中(WAIT)     │
  │                        │ deadline / retry / cfg     │
  │                        │                            │
  │ 主循环 txn_poll() ──── 检查超时: 未超时→继续等       │
  │                        ├─ 超时且有重发次数→再调 request()
  │                        └─ 重发用尽→回调 TIMEOUT      │
  │                        │                            │
  │ 收到帧 txn_on_rx() ── 按 cmd_id 逐槽调 match() ────│← 收到应答
  │        命中→释放槽位→回调 notify(result)            │
```

## 快速开始（移植 3 步）

1. **实现毫秒节拍**（任何能提供 ms 计数的东西都行）：

```c
#include "txn_port.h"

uint32_t txn_port_now_ms(void)
{
    return HAL_GetTick();   /* 裸机 HAL */
    /* 或 xTaskGetTickCount() * portTICK_PERIOD_MS; FreeRTOS */
}
```

2. **（可选）配置临界区**：如果在中断里调用 `txn_on_rx()`，需要保护槽位表：

```c
#define TXN_LOCK()   __disable_irq()
#define TXN_UNLOCK() __enable_irq()
```

3. **（可选）配置槽位数量**（默认 8，编译选项覆盖）：

```cmake
target_compile_definitions(txn_manager PUBLIC TXN_MAX_SLOTS=16)
```

然后就是标准用法：

```c
int main(void)
{
    txn_init();

    for (;;) {
        /* 任意时机提交事务 */
        if (有新命令要发) {
            txn_submit(&cfg);
        }

        /* 收到一帧（主循环轮询或中断） */
        if (收到一帧) {
            txn_on_rx(buf, len);
        }

        /* 周期驱动超时/重发，建议每 1~10ms 调一次 */
        txn_poll();
    }
}
```

## 提交一个事务

把配置打包进 `txn_req_cfg_t` 即可，栈上变量也没问题（提交时会拷贝）：

```c
static int read_version_request(void *ctx)
{
    return uart_send(ctx, "\xA5\x01", 2);   /* 0 = 发送成功 */
}

static txn_match_t read_version_match(const void *rx, size_t len, void *ctx)
{
    const uint8_t *b = rx;
    if (len < 3)                 return TXN_MATCH_MORE;  /* 不完整，继续等 */
    if (b[1] != 0x81)            return TXN_MATCH_MORE;  /* 不是我的应答 */
    if (b[2] != b[0] ^ 0xFF)     return TXN_MATCH_ABORT; /* 校验失败 */
    return TXN_MATCH_OK;
}

static void on_result(const txn_result_t *r)
{
    dev_t *dev = r->ctx;                      /* 靠 ctx 区分是哪个事务 */
    if (r->wait_ret == TXN_WAIT_RESP_OK) {
        handle_version(dev, r->rx, r->len);   /* 成功：应答数据在 rx/len */
    } else {
        log("cmd 0x%04X 失败: %s, request_ret=%d, 重发=%u 次",
            r->cmd_id, txn_wait_ret_str(r->wait_ret),
            r->request_ret, r->retry);
    }
}

txn_req_cfg_t cfg = {
    .cmd_id     = 0x0100,
    .request    = read_version_request,  /* 怎么发 */
    .match      = read_version_match,    /* 怎么判断是本事务的应答 */
    .notify     = on_result,             /* 成功/失败统一回调 */
    .ctx        = &dev,                  /* 透传给以上三个函数 */
    .timeout_ms = 100,                   /* 单次等待 100ms */
    .max_retry  = 2,                     /* 超时后最多重发 2 次 */
};

int handle = txn_submit(&cfg);
/* handle >= 0: 成功占用槽位
 * -1 表满  -2 同 cmd_id 仍在等待  -3 发送失败(会回调)  -4 参数非法 */
```

## 回调参数（txn_result_t）

成功和失败都走同一个 `notify` 回调，一次事务恰好回调一次：

| 字段 | 含义 |
|---|---|
| `cmd_id` | 事务命令ID |
| `ctx` | 提交时的用户上下文，**用它区分是哪个事务** |
| `wait_ret` | 等待的最终返回值（见下表） |
| `request_ret` | 最近一次 `request()` 发送函数的返回值，0 = 发送成功 |
| `retry` | 实际发生过的重发次数 |
| `rx` / `len` | 应答数据，仅 `RESP_OK` / `RESP_BAD` 时有效，否则为 NULL |

`wait_ret` 取值：

| 值 | 含义 |
|---|---|
| `TXN_WAIT_RESP_OK` | 收到有效应答，事务成功 |
| `TXN_WAIT_RESP_BAD` | 收到应答但 match 校验失败（坏帧在 `rx/len` 里） |
| `TXN_WAIT_TIMEOUT` | 超时且重发次数用尽 |
| `TXN_WAIT_TX_ERR` | `request()` 返回错误，未进入等待（错误码在 `request_ret`） |
| `TXN_WAIT_CANCELLED` | 被 `txn_abort()` 主动取消 |

## API 一览

| 函数 | 说明 |
|---|---|
| `txn_init()` | 初始化，清空槽位表，上电调一次 |
| `txn_submit(cfg)` | 提交事务，立即发送；返回句柄或负值错误码 |
| `txn_on_rx(rx, len)` | 收到一帧时调用，按 cmd_id 配对分发；返回 1=已消费该帧 |
| `txn_poll()` | 主循环周期调用，驱动超时与自动重发 |
| `txn_abort(handle)` | 主动取消，回调 `CANCELLED`；返回 1=确实取消了事务 |
| `txn_pending_count()` | 当前等待中的事务数 |
| `txn_wait_ret_str()` | 等待结果转字符串，便于打日志 |

## 演示程序

`main.c` 用虚拟时间模拟了一个主机-从机环境，覆盖 3 个典型场景：

1. **应答慢但未超时**：55ms 才回，100ms 超时 → 成功，只发一次
2. **两命令并发、应答乱序**：0x11 后发先回，0x10 先发后回 → 依然各自配对正确
3. **从机永不应答**：超时 20ms、重发 2 次 → 共发 3 次，回调 `TIMEOUT`

编译运行：

```bash
cmake -B build -S .
cmake --build build
build\Debug\transaction_manager_demo.exe   # Windows
ctest --test-dir build -C Debug            # 跑测试（MSVC 多配置需 -C Debug）
```

Linux/macOS 下把路径换成 `build/transaction_manager_demo` 即可（`ctest` 不需要 `-C`）。

## 注意事项

- **同一 `cmd_id` 串行**：一个命令还在等待时不允许再次提交（返回 -2），否则应答无法区分归属；不同 `cmd_id` 可以任意并发
- **`match` 返回值决定走向**：`MORE` = 不是我的继续等，`OK` = 完成，`ABORT` = 判失败（坏帧细节在回调里）
- **`match` 可为 NULL**：任意帧都算应答，只适合总线上没有其他流量的场景
- **回调内可以链式提交**：管理器先释放槽位再调回调，回调里直接 `txn_submit()` 下一条是安全的
- **中断里调 `txn_on_rx()`** 必须配置 `TXN_LOCK/UNLOCK`；`txn_poll()`、`txn_submit()` 建议放主循环
- **`txn_poll()` 调用频率**决定超时判断精度，建议 1~10ms 一次；两次调用间隔不要超过 `timeout_ms`
- **提交失败（-1/-2/-4）不占槽位、不回调**，只有真正占了槽位的事务才保证恰好回调一次
