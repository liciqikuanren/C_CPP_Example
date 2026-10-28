# event_bus — 轻量级 C 事件总线模块

一个面向嵌入式/裸机场景的静态分配事件总线，不使用 `malloc`，由三个核心对象组成：

| 对象 | 类型 | 职责 |
|------|------|------|
| 发布者 | `publisher_t` | 持有**枚举 ID**、**事件队列**、**订阅者链表首地址** |
| 订阅者 | `subscriber_t` | 自持**缓存队列**、**下一订阅者指针**（链表访问）、事件回调；**订阅动作由订阅者发起** |
| 事件总线引擎 | `event_bus_t` | 挂载发布者、承担“事件 ID → 发布者”倒推、`event_bus_process()` 驱动分发 |

## 1. 目录结构

```
event_bus/
├── CMakeLists.txt      # CMake 构建脚本
├── event_bus.h         # 类型定义 + API 声明
├── event_bus.c         # 实现
├── main.c              # 使用示例
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
    event_t         queue[SUB_QUEUE_LEN];    // 本地缓存队列
    uint8_t         head, tail, count;
    struct subscriber *next;                 // 下一个订阅者（链表）
} subscriber_t;

typedef struct publisher {
    event_id_t  id;                          // 枚举 ID（订阅者据此倒推本发布者）
    event_t     queue[PUB_QUEUE_LEN];        // 事件队列
    uint8_t     head, tail, count;
    struct subscriber *subs;                 // 订阅者链表首地址
} publisher_t;

typedef struct event_bus {
    publisher_t *pubs[EVENT_BUS_MAX_PUBS];   // 发布者表（用于事件 ID 倒推）
    uint8_t      pub_cnt;
    subscriber_t *any_subs;                  // EVENT_ID_ANY 通配订阅者链表首地址
} event_bus_t;
```

### 订阅方式：按事件倒推发布者

订阅由**订阅者**发起，不直接指定发布者：

```c
subscriber_subscribe(&bus, &s_key, EVENT_ID_KEY);   // 引擎倒推 -> key_pub
subscriber_subscribe(&bus, &s_tick, EVENT_ID_TIMER); // 引擎倒推 -> tick_pub
subscriber_subscribe(&bus, &s_log,  EVENT_ID_ANY);   // 通配     -> 总线链表
```

内部流程：

1. 引擎在发布者表中查找 `pub->id == id` 的发布者；
2. 找到后把订阅者**头插**进该发布者的 `subs` 链表；
3. `id == EVENT_ID_ANY` 时挂到总线的通配链表 `any_subs`（接收所有事件）。

约束（由单向 `next` 指针决定）：

- **一个订阅者同一时刻只属于一条链表**；重复/切换订阅会先自动摘旧链再挂新链。
- 需要同时接收多类事件时，请使用**多个订阅者对象**（可绑定同一回调），或直接用 `EVENT_ID_ANY`。
- 退订传具体 ID 只从该发布者摘除；传 `EVENT_ID_ANY` 表示全部摘除。

### 链表方向

- `publisher_t.subs`：**头插法**维护订阅者链表首地址，引擎从这里开始遍历。
- `subscriber_t.next`：指向下一个订阅者，形成单向链表；退订时摘链。
- `event_bus_t.any_subs`：通配订阅者链表首地址。

### 事件流转（两级缓存）

```
publisher_post()                event_bus_process()              handler
┌──────────────┐  phase 1     ┌──────────────┐   phase 2      ┌─────────┐
│ publisher 队列│ ───────────► │ subscriber 队列│ ────────────► │ callback│
└──────────────┘ 遍历 subs 链表└──────────────┘  pop 后回调    └─────────┘
                   + any_subs 通配链表
```

1. **phase 1**：引擎排空发布者队列，沿 `subs` 链表把事件推入订阅者缓存队列，同时推入通配链表。
2. **phase 2**：再次沿链表逐个 `pop` 订阅者队列，调用其 `handler`。

订阅者队列满时事件被**丢弃**（`subscriber_push()` 返回 `false`），不阻塞发布者。

## 3. 可配置项（编译期宏）

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `EVENT_PAYLOAD_MAX` | 16 | 事件载荷最大字节数 |
| `PUB_QUEUE_LEN` | 8 | 发布者队列深度 |
| `SUB_QUEUE_LEN` | 8 | 订阅者缓存队列深度 |
| `EVENT_BUS_MAX_PUBS` | 8 | 引擎可挂载的发布者数量 |
| `EVENT_BUS_LOCK()` / `EVENT_BUS_UNLOCK()` | 空 | 临界区钩子，多线程/中断环境自行填入关中断或加锁 |

例如在 STM32 上可这样配置：

```c
#define EVENT_BUS_LOCK()   __disable_irq()
#define EVENT_BUS_UNLOCK() __enable_irq()
#include "event_bus.h"
```

## 4. API 一览

### 订阅者 `subscriber_t`（订阅动作入口）

| 函数 | 说明 |
|------|------|
| `void subscriber_init(subscriber_t *sub, event_handler_t handler)` | 初始化，只绑定回调，不挂链 |
| `bool subscriber_subscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)` | **按事件 ID 倒推发布者并挂链**；`EVENT_ID_ANY` 挂通配链表 |
| `void subscriber_unsubscribe(event_bus_t *bus, subscriber_t *sub, event_id_t id)` | **按事件 ID 倒推发布者并摘链**；`EVENT_ID_ANY` 全部摘除 |
| `bool subscriber_push(subscriber_t *sub, const event_t *ev)` | 事件写入缓存队列 |
| `bool subscriber_pop(subscriber_t *sub, event_t *out)` | 从缓存队列取事件 |
| `bool subscriber_empty(const subscriber_t *sub)` | 缓存队列是否为空 |

### 发布者 `publisher_t`

| 函数 | 说明 |
|------|------|
| `void publisher_init(publisher_t *pub, event_id_t id)` | 初始化，绑定枚举 ID |
| `bool publisher_post(publisher_t *pub, const void *data, uint16_t len)` | 发布事件：只填发布者实例、数据、长度，**事件 ID 自动取发布者的枚举 ID** |
| `void publisher_subscribe(publisher_t *pub, subscriber_t *sub)` | 底层挂链原语（业务代码建议用 `subscriber_subscribe`） |
| `void publisher_unsubscribe(publisher_t *pub, subscriber_t *sub)` | 底层摘链原语 |
| `bool publisher_empty(const publisher_t *pub)` | 队列是否为空 |

### 引擎 `event_bus_t`

| 函数 | 说明 |
|------|------|
| `void event_bus_init(event_bus_t *bus)` | 初始化引擎 |
| `bool event_bus_add_publisher(event_bus_t *bus, publisher_t *pub)` | 挂载发布者（挂载后才能被倒推查到） |
| `void event_bus_process(event_bus_t *bus)` | 运行一轮：分发 + 回调 |

## 5. 使用示例

```c
event_bus_t  bus;
publisher_t  key_pub;
subscriber_t s_key, s_log;

/* 1. 初始化引擎与发布者并挂载 */
event_bus_init(&bus);
publisher_init(&key_pub, EVENT_ID_KEY);
event_bus_add_publisher(&bus, &key_pub);

/* 2. 初始化订阅者：只绑定回调 */
subscriber_init(&s_key, on_key);
subscriber_init(&s_log, on_log);

/* 3. 订阅：传事件 ID，引擎倒推发布者后挂链 */
subscriber_subscribe(&bus, &s_key, EVENT_ID_KEY);  // -> key_pub
subscriber_subscribe(&bus, &s_log, EVENT_ID_ANY);  // -> 通配链表（全监听）

/* 4. 任意上下文发布事件：只给发布者 + 数据 + 长度，ID 自动填入 */
publisher_post(&key_pub, "A", 1);

while (1) {                 // 主循环 / 任务中周期调用
    event_bus_process(&bus);
}

/* 5. 退订 */
subscriber_unsubscribe(&bus, &s_key, EVENT_ID_KEY);
```

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

- **静态分配**：所有队列/链表节点均为结构体内嵌数组与指针，无动态内存，适合裸机与 RTOS。
- **按事件倒推发布者**：订阅者只说“我要什么事件”，由引擎查发布者表定位发布者并挂链，发布者与订阅者之间零耦合。
- **两级队列**：发布者队列隔离“生产上下文”，订阅者队列隔离“消费上下文”，双方可运行在不同优先级/中断上下文。
- **单链表单归属**：一个订阅者只在一条链表中（`next` 唯一），换订阅自动摘旧挂新，避免链表交叉成环。
- **通配订阅**：`EVENT_ID_ANY` 挂总线级 `any_subs` 链表，一次订阅监听全部事件。
- **可重入保护**：通过 `EVENT_BUS_LOCK/UNLOCK` 宏注入临界区，默认无操作（单线程安全）。
- **队列满丢弃**：保证发布者非阻塞，返回值可用来统计丢包。
