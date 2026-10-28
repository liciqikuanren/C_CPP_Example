# event_bus — 轻量级 C 事件总线模块

一个面向嵌入式/裸机场景的静态分配事件总线，不使用 `malloc`，由三个核心对象组成：

| 对象 | 类型 | 职责 |
|------|------|------|
| 发布者 | `publisher_t` | 只持**枚举 ID** 与**订阅者链表首地址**，不自持队列；与事件 ID **一一对应** |
| 订阅者 | `subscriber_t` | 自持**缓存队列**、**下一订阅者指针**（链表访问）、事件回调；**订阅动作由订阅者发起** |
| 事件总线引擎 | 内部单例 | 持有 **ID→发布者查表数组**、**全局唯一事件总队列**、通配链表；`event_bus_process()` 驱动分发 |

**单实例**：一个工程只有一个事件总线，引擎结构体与实例（`static g_bus`）隐藏在 `event_bus.c` 内部，所有引擎 API 无实例参数。

**直接查表**：发布者与事件 ID 一一对应，挂载时以 ID 为下标写入 `pub_by_id[EVENT_ID_MAX]`；分发、订阅、退订时 `pub_by_id[id]` 直接取出发布者实例，**O(1) 命中，无遍历**。

**总队列**：发布者不自持队列，事件统一投递到总线唯一总队列；扫描时查表取发布者，再检索其订阅者链表完成分发。

## 1. 目录结构

```
event_bus/
├── CMakeLists.txt      # CMake 构建脚本
├── event_bus.h         # 类型定义 + API 声明
├── event_bus.c         # 实现（引擎单例与查表数组在此文件内部）
├── main.c              # 使用示例
└── README.md           # 本文档
```

## 2. 数据结构

```c
typedef struct event {
    event_id_t  id;                          // 事件枚举 ID（即查表下标）
    uint16_t    len;                         // 有效载荷长度
    uint8_t     payload[EVENT_PAYLOAD_MAX];  // 载荷
} event_t;

typedef struct subscriber {
    event_handler_t handler;                 // 事件回调
    event_t         queue[SUB_QUEUE_LEN];    // 本地缓存队列（默认深度 1）
    uint8_t         head, tail, count;
    struct subscriber *next;                 // 下一个订阅者（链表）
} subscriber_t;

typedef struct publisher {
    event_id_t  id;                          // 枚举 ID（查表下标，与 ID 一一对应）
    struct subscriber *subs;                 // 订阅者链表首地址（无事件队列）
} publisher_t;

/* 隐藏于 event_bus.c 内部的引擎单例 */
typedef struct event_bus {
    publisher_t  *pub_by_id[EVENT_ID_MAX];   // 事件 ID -> 发布者 直接查表（O(1)）
    subscriber_t *any_subs;                  // EVENT_ID_ANY 通配订阅者链表首地址
    event_t       queue[EVENT_QUEUE_LEN];    // 全局唯一事件总队列
    uint8_t       head, tail, count;
} event_bus_t;
static event_bus_t g_bus;                    // 全局唯一实例（静态内存）
```

### 订阅方式：按事件 ID 查表定位发布者

订阅由**订阅者**发起，不直接指定发布者：

```c
subscriber_subscribe(&s_key, EVENT_ID_KEY);    // 查表 -> key_pub
subscriber_subscribe(&s_tick, EVENT_ID_TIMER); // 查表 -> tick_pub
subscriber_subscribe(&s_log,  EVENT_ID_ANY);   // 通配 -> 总线链表
```

内部流程：

1. 直接取 `g_bus.pub_by_id[id]` 拿到发布者实例（ID 即数组下标，一一对应，O(1)）；
2. 拿到后把订阅者**头插**进该发布者的 `subs` 链表；
3. `id == EVENT_ID_ANY` 时挂到总线通配链表 `any_subs`（接收所有事件）。

约束（由单向 `next` 指针决定）：

- **一个订阅者同一时刻只属于一条链表**；重复/切换订阅会先自动摘旧链再挂新链。
- 需要同时接收多类事件时，请使用**多个订阅者对象**（可绑定同一回调），或直接用 `EVENT_ID_ANY`。
- 退订传具体 ID 只从该发布者摘除；传 `EVENT_ID_ANY` 表示全部摘除。

### 链表方向

- `publisher_t.subs`：**头插法**维护订阅者链表首地址，引擎分发时从这里开始遍历。
- `subscriber_t.next`：指向下一个订阅者，形成单向链表；退订时摘链。
- 引擎 `any_subs`：通配订阅者链表首地址。

### 事件流转（唯一总队列 + 订阅者深度1缓存）

```
publisher_post()              event_bus_process()             handler
┌──────────────┐  enqueue    ┌──────────────┐   阶段2       ┌─────────┐
│  引擎总队列   │ ──────────► │ subscriber 队列│ ───────────► │ callback│
└──────────────┘  阶段1:ID查表└──────────────┘  pop 后回调   └─────────┘
                 发布者->subs 链表 + any_subs 通配链表
```

1. **阶段 1**：排空总队列，对每个事件用 `g_bus.pub_by_id[ev.id]` 直接取出发布者（O(1)），沿 `subs` 链表把事件推入订阅者缓存队列，同时推入通配链表。
2. **阶段 2**：遍历各发布者链表与通配链表，逐个 `pop` 订阅者缓存，调用其 `handler`。

默认订阅者缓存队列深度为 **1**：阶段 1 入队、阶段 2 立即出队回调，**每轮扫描即执行订阅者回调**，只缓存最新事件；队列满时覆盖最旧事件（始终保留最新）。

> 深度为 1 时，若一轮扫描前发布了多个同订阅者事件，只有**最后一个**会被回调（更早的被覆盖）。需要完整缓存历史事件时可在编译前定义 `SUB_QUEUE_LEN` 加大深度。

### 发布前置条件

`publisher_post()` 要求发布者**已挂载**（`event_bus_add_publisher` 写表命中且实例一致），否则返回 `false`；同一 ID 挂载第二个不同实例也会被拒绝，保证查表的一一对应关系。

## 3. 可配置项（编译期宏）

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EVENT_PAYLOAD_MAX` | 16 | 事件载荷最大字节数 |
| `EVENT_QUEUE_LEN` | 8 | **引擎事件总队列深度（全局唯一一份）** |
| `SUB_QUEUE_LEN` | 1 | 订阅者缓存队列深度；1 = 扫描即回调，仅存最新事件（最小内存） |
| `EVENT_BUS_LOCK()` / `EVENT_BUS_UNLOCK()` | 空 | 临界区钩子，多线程/中断环境自行填入关中断或加锁 |

> 发布者挂载容量 = `EVENT_ID_MAX`（查表数组大小由枚举决定，每 ID 至多一个发布者，无需单独配置数量宏）。

例如在 STM32 上可这样配置：

```c
#define EVENT_BUS_LOCK()   __disable_irq()
#define EVENT_BUS_UNLOCK() __enable_irq()
#include "event_bus.h"
```

### 内存占用估算（默认配置，64 位机）

| 项 | 大小 |
|----|------|
| 引擎单例 | 总队列 `8 × sizeof(event_t)` ≈ **192 字节** + ID 查表数组 `EVENT_ID_MAX × 指针`（40 字节）/通配指针 ≈ 约 **248 字节（全工程一份）** |
| 每个订阅者 | 缓存 1×24 + 回调/指针等 ≈ **约 48 字节** |
| 每个发布者 | 枚举 ID + 链表指针 ≈ **约 16 字节**（无队列、无归属指针） |

对比每发布者一份队列的方案：发布者数量越多，节省越明显。

## 4. API 一览

### 引擎（单例，无实例参数）

| 函数 | 说明 |
|------|------|
| `void event_bus_init(void)` | 初始化引擎（清空总队列、ID 查表数组、通配链表） |
| `bool event_bus_add_publisher(publisher_t *pub)` | 挂载发布者：**以 `pub->id` 为下标直接写表**（同一 ID 不允许挂两个实例） |
| `bool event_bus_empty(void)` | 总队列是否为空 |
| `void event_bus_process(void)` | 运行一轮：总队列分发 + 回调 |

### 订阅者 `subscriber_t`（订阅动作入口）

| 函数 | 说明 |
|------|------|
| `void subscriber_init(subscriber_t *sub, event_handler_t handler)` | 初始化，只绑定回调，不挂链 |
| `bool subscriber_subscribe(subscriber_t *sub, event_id_t id)` | **按事件 ID 查表取发布者并挂链（O(1)）**；`EVENT_ID_ANY` 挂通配链表 |
| `void subscriber_unsubscribe(subscriber_t *sub, event_id_t id)` | **按事件 ID 查表取发布者并摘链**；`EVENT_ID_ANY` 全部摘除 |
| `bool subscriber_push(subscriber_t *sub, const event_t *ev)` | 事件写入缓存队列（满则覆盖最旧） |
| `bool subscriber_pop(subscriber_t *sub, event_t *out)` | 从缓存队列取事件 |
| `bool subscriber_empty(const subscriber_t *sub)` | 缓存队列是否为空 |

### 发布者 `publisher_t`

| 函数 | 说明 |
|------|------|
| `void publisher_init(publisher_t *pub, event_id_t id)` | 初始化，绑定枚举 ID |
| `bool publisher_post(publisher_t *pub, const void *data, uint16_t len)` | 发布事件：只填发布者实例、数据、长度，**ID 自动填入，进入总线总队列**（需已挂载） |
| `void publisher_subscribe(publisher_t *pub, subscriber_t *sub)` | 底层挂链原语（业务代码建议用 `subscriber_subscribe`） |
| `void publisher_unsubscribe(publisher_t *pub, subscriber_t *sub)` | 底层摘链原语 |

## 5. 使用示例

```c
publisher_t  key_pub;
subscriber_t s_key, s_log;

/* 1. 初始化引擎（单例），初始化发布者并挂载（ID 直接写查表数组） */
event_bus_init();
publisher_init(&key_pub, EVENT_ID_KEY);
event_bus_add_publisher(&key_pub);

/* 2. 初始化订阅者：只绑定回调 */
subscriber_init(&s_key, on_key);
subscriber_init(&s_log, on_log);

/* 3. 订阅：传事件 ID，查表取发布者后挂链 */
subscriber_subscribe(&s_key, EVENT_ID_KEY);  // -> key_pub
subscriber_subscribe(&s_log, EVENT_ID_ANY);  // -> 通配链表（全监听）

/* 4. 任意上下文发布事件：进入总线总队列 */
publisher_post(&key_pub, "A", 1);

while (1) {                 // 主循环 / 任务中周期调用
    event_bus_process();    // 排空总队列 -> ID 查表 -> 链表分发 -> 回调
}

/* 5. 退订 */
subscriber_unsubscribe(&s_key, EVENT_ID_KEY);
```

事件枚举按需在 `event_bus.h` 中扩展——新增一个 ID，查表数组 `pub_by_id[EVENT_ID_MAX]` 自动随之变大，无需改配置：

```c
typedef enum {
    EVENT_ID_NONE = 0,
    EVENT_ID_KEY,
    EVENT_ID_TIMER,
    EVENT_ID_UART,
    EVENT_ID_SENSOR,
    EVENT_ID_MAX
} event_id_t;
```

## 6. 编译与运行（CMake）

```bash
# 配置
cmake -S . -B build

# 编译（多配置生成器如 VS 需指定配置）
cmake --build build --config Release

# 运行示例
./build/Release/event_bus_demo.exe      # Linux/macOS 或单配置生成器
./build/Debug/event_bus_demo.exe        # Visual Studio 生成器

# 运行测试（VS 多配置需 -C）
ctest --test-dir build -C Release --output-on-failure
```

产物：

- `event_bus.lib`（静态库，可被其他工程链接）
- `event_bus_demo.exe`（示例程序）

作为子模块集成到其他工程：

```cmake
add_subdirectory(event_bus)
target_link_libraries(your_target PRIVATE event_bus)
```

## 7. 设计要点

- **单实例引擎**：一个工程一个事件总线，引擎隐藏为 `event_bus.c` 内部 `static g_bus`，API 无实例参数。
- **ID 直接查表**：发布者与事件 ID 一一对应，`pub_by_id[id]` 下标直取实例（挂载/分发/订阅/退订均为 O(1)），无链表或循环查找；容量即枚举数量，同一 ID 挂第二个实例会被拒绝。
- **唯一总队列**：全工程共用一份 `EVENT_QUEUE_LEN` 深度的事件队列，挂再多发布者也不增加队列开销。
- **静态分配**：总队列、查表数组、订阅者缓存、链表节点均为内嵌数组与指针，无动态内存，适合裸机与 RTOS。
- **订阅者只说需求**：订阅者只声明“我要什么事件”，由查表定位发布者，再走其订阅者链表分发，发布者与订阅者之间零耦合。
- **低内存缓存**：订阅者缓存队列默认深度 1（约 24 字节/订阅者），扫描一轮即入队→出队→回调，只保留最新事件；需要时用 `SUB_QUEUE_LEN` 换深度。
- **单链表单归属**：一个订阅者只在一条链表中（`next` 唯一），换订阅自动摘旧挂新，避免链表交叉成环。
- **通配订阅**：`EVENT_ID_ANY` 挂引擎级 `any_subs` 链表，一次订阅监听全部事件。
- **可重入保护**：通过 `EVENT_BUS_LOCK/UNLOCK` 宏注入临界区，默认无操作（单线程安全）。
- **满载策略**：订阅者缓存满覆盖最旧、保留最新；引擎总队列满丢弃新事件，保证发布非阻塞。
