# event_bus — 轻量级 C 事件总线模块

一个面向嵌入式/裸机场景的静态分配事件总线，不使用 `malloc`，由三个核心对象组成：

| 对象 | 类型 | 职责 |
|------|------|------|
| 发布者 | `publisher_t` | 持**枚举 ID**、**订阅者链表首地址**、**所属引擎指针**，不自持队列 |
| 订阅者 | `subscriber_t` | 自持**缓存队列**、**下一订阅者指针**（链表访问）、事件回调；**订阅动作由订阅者发起** |
| 事件总线引擎 | `event_bus_t` | 每实例持**唯一事件总队列**、发布者表（ID 倒推）、通配链表；`event_bus_process(bus)` 驱动分发 |

**多实例**：`event_bus_t` 允许多个实例，每个实例的队列、发布者表、通配链表完全独立，事件互不串扰；实例由调用方分配（建议 `static`/全局，保持纯静态内存）。

**总队列**：发布者不自持队列，事件统一投递到**所属引擎**的唯一总队列；扫描时用事件 ID 倒推发布者，再检索其订阅者链表完成分发。一个引擎无论挂多少发布者，事件缓冲只有一份。

## 1. 目录结构

```
event_bus/
├── CMakeLists.txt      # CMake 构建脚本
├── event_bus.h         # 类型定义 + API 声明
├── event_bus.c         # 实现
├── main.c              # 使用示例（含双引擎实例演示）
└── README.md           # 本文档
```

## 2. 数据结构

```c
typedef struct event {
    event_id_t  id;                          // 事件枚举 ID
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
    event_id_t  id;                          // 枚举 ID（订阅者据此倒推本发布者）
    struct subscriber *subs;                 // 订阅者链表首地址（无事件队列）
    event_bus_t *bus;                        // 所属引擎（挂载时写入，post 的投递目标）
} publisher_t;

typedef struct event_bus {
    publisher_t  *pubs[EVENT_BUS_MAX_PUBS];  // 发布者表（事件 ID -> 发布者 倒推）
    uint8_t       pub_cnt;
    subscriber_t *any_subs;                  // EVENT_ID_ANY 通配订阅者链表首地址
    event_t       queue[EVENT_QUEUE_LEN];    // 本实例的事件总队列
    uint8_t       head, tail, count;
} event_bus_t;
```

### 订阅方式：按事件倒推发布者

订阅由**订阅者**发起，不直接指定发布者：

```c
subscriber_subscribe(&bus, &s_key, EVENT_ID_KEY);    // 引擎倒推 -> key_pub
subscriber_subscribe(&bus, &s_tick, EVENT_ID_TIMER); // 引擎倒推 -> tick_pub
subscriber_subscribe(&bus, &s_log,  EVENT_ID_ANY);   // 通配     -> 引擎链表
```

内部流程：

1. 在指定引擎的发布者表中查找 `pub->id == id` 的发布者；
2. 找到后把订阅者**头插**进该发布者的 `subs` 链表；
3. `id == EVENT_ID_ANY` 时挂到该引擎通配链表 `any_subs`（接收该引擎所有事件）。

约束（由单向 `next` 指针决定）：

- **一个订阅者同一时刻只属于一条链表**；重复/切换订阅会先自动摘旧链再挂新链。
- 需要同时接收多类事件时，请使用**多个订阅者对象**（可绑定同一回调），或直接用 `EVENT_ID_ANY`。
- 退订传具体 ID 只从该发布者摘除；传 `EVENT_ID_ANY` 表示该引擎内全部摘除。

### 链表方向

- `publisher_t.subs`：**头插法**维护订阅者链表首地址，引擎分发时从这里开始遍历。
- `subscriber_t.next`：指向下一个订阅者，形成单向链表；退订时摘链。
- `event_bus_t.any_subs`：该实例的通配订阅者链表首地址。

### 事件流转（每引擎唯一总队列 + 订阅者深度1缓存）

```
publisher_post()              event_bus_process(bus)          handler
┌──────────────┐  enqueue    ┌──────────────┐   阶段2       ┌─────────┐
│ 引擎总队列    │ ──────────► │ subscriber 队列│ ───────────► │ callback│
└──────────────┘  阶段1:ID倒推└──────────────┘  pop 后回调   └─────────┘
                 发布者->subs 链表 + any_subs 通配链表
```

1. **阶段 1**：排空总队列，对每个事件用其 ID 倒推发布者，沿 `subs` 链表把事件推入订阅者缓存队列，同时推入通配链表。
2. **阶段 2**：遍历各发布者链表与通配链表，逐个 `pop` 订阅者缓存，调用其 `handler`。

默认订阅者缓存队列深度为 **1**：阶段 1 入队、阶段 2 立即出队回调，**每轮扫描即执行订阅者回调**，只缓存最新事件；队列满时覆盖最旧事件（始终保留最新）。

> 深度为 1 时，若一轮扫描前发布了多个同订阅者事件，只有**最后一个**会被回调（更早的被覆盖）。需要完整缓存历史事件时可在编译前定义 `SUB_QUEUE_LEN` 加大深度。

### 发布投递：归属引擎

`event_bus_add_publisher(bus, pub)` 挂载时把 `pub->bus = bus` 记录归属；
`publisher_post(pub, data, len)` 据此把事件投递到 `pub->bus->queue`，
**发布时无需显式传引擎**；发布者未挂载时 `publisher_post` 返回 `false`。

## 3. 可配置项（编译期宏）

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EVENT_PAYLOAD_MAX` | 16 | 事件载荷最大字节数 |
| `EVENT_QUEUE_LEN` | 8 | **引擎事件总队列深度（每个引擎实例一份）** |
| `SUB_QUEUE_LEN` | 1 | 订阅者缓存队列深度；1 = 扫描即回调，仅存最新事件（最小内存） |
| `EVENT_BUS_MAX_PUBS` | 8 | 单个引擎可挂载的发布者数量 |
| `EVENT_BUS_LOCK()` / `EVENT_BUS_UNLOCK()` | 空 | 临界区钩子，多线程/中断环境自行填入关中断或加锁 |

例如在 STM32 上可这样配置：

```c
#define EVENT_BUS_LOCK()   __disable_irq()
#define EVENT_BUS_UNLOCK() __enable_irq()
#include "event_bus.h"
```

### 内存占用估算（默认配置，64 位机）

| 项 | 大小 |
|----|------|
| 每个引擎实例 | 总队列 `8 × sizeof(event_t)` ≈ **192 字节** + 发布者表/通配指针 ≈ 约 **240 字节** |
| 每个订阅者 | 缓存 1×24 + 回调/指针等 ≈ **约 48 字节** |
| 每个发布者 | 枚举 ID + 链表指针 + 引擎指针 ≈ **约 24 字节**（无队列） |

对比每发布者一份队列的方案：单引擎内发布者数量增加不再增加队列开销；需要几个引擎就分配几个实例，内存按需可控。

## 4. API 一览

### 引擎 `event_bus_t`（多实例，实例由调用方分配）

| 函数 | 说明 |
|------|------|
| `void event_bus_init(event_bus_t *bus)` | 初始化引擎（清空总队列、发布者表、通配链表） |
| `bool event_bus_add_publisher(event_bus_t *bus, publisher_t *pub)` | 挂载发布者（可被倒推查到），并记录 `pub->bus` 归属 |
| `bool event_bus_empty(const event_bus_t *bus)` | 该实例总队列是否为空 |
| `void event_bus_process(event_bus_t *bus)` | 运行一轮：总队列分发 + 回调 |

### 订阅者 `subscriber_t`（订阅动作入口）

| 函数 | 说明 |
|------|------|
| `void subscriber_init(subscriber_t *sub, event_handler_t handler)` | 初始化，只绑定回调，不挂链 |
| `bool subscriber_subscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)` | **在指定引擎上按事件 ID 倒推发布者并挂链**；`EVENT_ID_ANY` 挂通配链表 |
| `void subscriber_unsubscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)` | **按事件 ID 倒推发布者并摘链**；`EVENT_ID_ANY` 全部摘除 |
| `bool subscriber_push(subscriber_t *sub, const event_t *ev)` | 事件写入缓存队列（满则覆盖最旧） |
| `bool subscriber_pop(subscriber_t *sub, event_t *out)` | 从缓存队列取事件 |
| `bool subscriber_empty(const subscriber_t *sub)` | 缓存队列是否为空 |

### 发布者 `publisher_t`

| 函数 | 说明 |
|------|------|
| `void publisher_init(publisher_t *pub, event_id_t id)` | 初始化，绑定枚举 ID |
| `bool publisher_post(publisher_t *pub, const void *data, uint16_t len)` | 发布事件：只填发布者实例、数据、长度，**ID 自动填入，投递到所属引擎总队列** |
| `void publisher_subscribe(publisher_t *pub, subscriber_t *sub)` | 底层挂链原语（业务代码建议用 `subscriber_subscribe`） |
| `void publisher_unsubscribe(publisher_t *pub, subscriber_t *sub)` | 底层摘链原语 |

## 5. 使用示例

```c
static event_bus_t  bus;                    /* 引擎实例（静态分配，允许多个） */
publisher_t  key_pub;
subscriber_t s_key, s_log;

/* 1. 初始化引擎，初始化发布者并挂载 */
event_bus_init(&bus);
publisher_init(&key_pub, EVENT_ID_KEY);
event_bus_add_publisher(&bus, &key_pub);    /* 记录归属，之后 post 无需传 bus */

/* 2. 初始化订阅者：只绑定回调 */
subscriber_init(&s_key, on_key);
subscriber_init(&s_log, on_log);

/* 3. 订阅：传事件 ID，引擎倒推发布者后挂链 */
subscriber_subscribe(&bus, &s_key, EVENT_ID_KEY);  // -> key_pub
subscriber_subscribe(&bus, &s_log, EVENT_ID_ANY);  // -> 通配链表（全监听）

/* 4. 任意上下文发布事件：投递到所属引擎总队列 */
publisher_post(&key_pub, "A", 1);

while (1) {                 // 主循环 / 任务中周期调用
    event_bus_process(&bus);   // 排空总队列 -> ID 倒推 -> 链表分发 -> 回调
}

/* 5. 退订 */
subscriber_unsubscribe(&bus, &s_key, EVENT_ID_KEY);
```

多实例：再定义一个 `static event_bus_t bus2;` 并重复上述步骤即可，两个引擎互不影响。

事件枚举按需在 `event_bus.h` 中扩展：

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

- **多实例引擎**：`event_bus_t` 实例由调用方分配（建议 static/全局），每个实例队列/发布者表/通配链表独立，适合“控制总线 / 调试总线”等按域隔离场景。
- **唯一总队列**：每个引擎内所有发布者共用一份 `EVENT_QUEUE_LEN` 深度的事件队列，挂再多发布者也不增加队列开销。
- **静态分配**：总队列、订阅者缓存、链表节点均为内嵌数组与指针，无动态内存，适合裸机与 RTOS。
- **按事件倒推发布者**：订阅者只说“我要什么事件”；扫描时用事件 ID 查发布者表定位发布者，再走其订阅者链表分发，发布者与订阅者之间零耦合。
- **归属记录**：挂载时写入 `pub->bus`，发布时免传引擎参数，一处挂载处处可用。
- **低内存缓存**：订阅者缓存队列默认深度 1（约 24 字节/订阅者），扫描一轮即入队→出队→回调，只保留最新事件；需要时用 `SUB_QUEUE_LEN` 换深度。
- **单链表单归属**：一个订阅者只在一条链表中（`next` 唯一），换订阅自动摘旧挂新，避免链表交叉成环。
- **通配订阅**：`EVENT_ID_ANY` 挂引擎级 `any_subs` 链表，一次订阅监听该引擎全部事件。
- **可重入保护**：通过 `EVENT_BUS_LOCK/UNLOCK` 宏注入临界区，默认无操作（单线程安全）。
- **满载策略**：订阅者缓存满覆盖最旧、保留最新；引擎总队列满丢弃新事件，保证发布非阻塞。
