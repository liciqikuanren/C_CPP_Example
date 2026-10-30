#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "state_machine.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>
#endif

/* 本 Demo 对应“状态机测试Dmoe.drawio”：
 *   IDLE --start--> UP --stop--> IDLE
 *   UP --计数到达10--> DOWN --计数回到0--> IDLE
 *   UP / DOWN 在每个 500ms 节拍里自增或自减一次（由 on_run 实现）
 */

/* 计数上限，UP 状态到达该值后转入 DOWN */
#define COUNT_MAX 10
/* 计数下限，DOWN 状态回落到该值后回到 IDLE */
#define COUNT_MIN 0
/* 状态机节拍周期，单位 ms，对应图中的“每 500ms” */
#define TICK_INTERVAL_MS 500u
/* 主循环轮询键盘的间隔，单位 ms */
#define INPUT_POLL_MS 20u

/* 演示用状态编号，需与 state_names 下标一一对应 */
enum {
    STATE_IDLE, /* 空闲：等待 start 命令 */
    STATE_UP,   /* 上升：每个节拍计数加一 */
    STATE_DOWN  /* 下降：每个节拍计数减一 */
};

/* 演示用事件编号，需与 event_names 下标一一对应 */
enum {
    EVENT_START,     /* 收到 start 命令 */
    EVENT_STOP,      /* 收到 stop 命令 */
    EVENT_REACH_MAX, /* 计数到达上限 */
    EVENT_REACH_MIN  /* 计数回落到下限 */
};

/* 状态名称表，下标即状态编号，仅用于日志打印 */
static const char *const state_names[] = {
    "IDLE", "UP", "DOWN"
};

/* 事件名称表，下标即事件编号，仅用于日志打印 */
static const char *const event_names[] = {
    "START", "STOP", "REACH_MAX", "REACH_MIN"
};

/* 演示用上下文：保存计数与节拍统计 */
typedef struct {
    int count; /* 当前计数值，UP 状态自增，DOWN 状态自减 */
    int ticks; /* 已执行的运行节拍总数 */
} counter_ctx_t;

/* 从状态机取出用户上下文并转换为 counter_ctx_t 指针。
 * 先校验上下文大小，防止类型不匹配导致越界访问。 */
static counter_ctx_t *ctx_of(state_machine_t *machine)
{
    assert(state_machine_user_size(machine) == sizeof(counter_ctx_t));
    return (counter_ctx_t *)state_machine_user(machine);
}

/* IDLE 状态的进入回调：提示等待 start 命令 */
static void idle_enter(state_machine_t *machine)
{
    (void)machine;
    printf("[IDLE] 进入空闲，等待 start 命令\n");
}

/* IDLE 状态的退出回调：提示离开空闲 */
static void idle_exit(state_machine_t *machine)
{
    (void)machine;
    printf("[IDLE] 离开空闲\n");
}

/* UP 状态的进入回调：打印进入时的计数值 */
static void up_enter(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);
    printf("[UP] 进入上升计数，当前计数=%d\n", ctx->count);
}

/* UP 状态的退出回调：打印退出时的计数值 */
static void up_exit(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);
    printf("[UP] 退出上升计数，当前计数=%d\n", ctx->count);
}

/* UP 状态的运行回调：每个节拍计数加一并检查是否到达上限 */
static void up_run(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);

    ctx->ticks++;
    ctx->count++;
    printf("[UP] 节拍=%d 计数=%d\n", ctx->ticks, ctx->count);

    /* 计数到达上限时投递事件，进入 DOWN 状态 */
    if (ctx->count >= COUNT_MAX) {
        state_machine_post(machine, EVENT_REACH_MAX);
    }
}

/* DOWN 状态的进入回调：打印进入时的计数值 */
static void down_enter(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);
    printf("[DOWN] 进入下降计数，当前计数=%d\n", ctx->count);
}

/* DOWN 状态的退出回调：打印退出时的计数值 */
static void down_exit(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);
    printf("[DOWN] 退出下降计数，当前计数=%d\n", ctx->count);
}

/* DOWN 状态的运行回调：每个节拍计数减一并检查是否回到下限 */
static void down_run(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);

    ctx->ticks++;
    ctx->count--;
    printf("[DOWN] 节拍=%d 计数=%d\n", ctx->ticks, ctx->count);

    /* 计数回落到下限时投递事件，回到 IDLE 状态 */
    if (ctx->count <= COUNT_MIN) {
        state_machine_post(machine, EVENT_REACH_MIN);
    }
}

/* EVENT_START 的转移动作：计数清零并打印当前状态 */
static void act_start(state_machine_t *machine)
{
    counter_ctx_t *ctx = ctx_of(machine);

    ctx->count = COUNT_MIN;
    printf("[动作] start 于 %s：计数清零\n",
           state_machine_state_name(machine, state_machine_current(machine)));
}

/* EVENT_STOP 的转移动作：打印当前状态 */
static void act_stop(state_machine_t *machine)
{
    printf("[动作] stop 于 %s：结束上升计数\n",
           state_machine_state_name(machine, state_machine_current(machine)));
}

/* EVENT_REACH_MAX 的转移动作：提示即将切换为下降计数 */
static void act_reach_max(state_machine_t *machine)
{
    printf("[动作] 计数到达上限 %d（状态 %s），准备转入下降\n",
           COUNT_MAX, state_machine_state_name(machine, state_machine_current(machine)));
}

/* EVENT_REACH_MIN 的转移动作：提示即将回到空闲 */
static void act_reach_min(state_machine_t *machine)
{
    printf("[动作] 计数回到下限 %d（状态 %s），准备回到空闲\n",
           COUNT_MIN, state_machine_state_name(machine, state_machine_current(machine)));
}

/* 状态表：每行依次为 状态编号、进入回调、退出回调、运行回调。
 * IDLE 没有周期行为，运行回调填 NULL，库会自动跳过。 */
static const state_machine_state_t states[] = {
    { STATE_IDLE, idle_enter, idle_exit, NULL     },
    { STATE_UP,   up_enter,   up_exit,   up_run   },
    { STATE_DOWN, down_enter, down_exit, down_run }
};

/* 转移表：每行依次为 当前状态、触发事件、下一状态、转移动作。
 * 图中 UP / DOWN 上的“每 500ms+1”自环由状态运行回调 on_run 实现，
 * 因此这里只描述由事件触发的状态切换。 */
static const state_machine_transition_t transitions[] = {
    { STATE_IDLE, EVENT_START,     STATE_UP,   act_start },
    { STATE_UP,   EVENT_STOP,      STATE_IDLE, act_stop },
    { STATE_UP,   EVENT_REACH_MAX, STATE_DOWN, act_reach_max },
    { STATE_DOWN, EVENT_REACH_MIN, STATE_IDLE, act_reach_min }
};

/* 状态机静态配置：状态表、转移表、名称表以及初始状态 */
static const state_machine_config_t config = {
    states,                                       /* 状态表首地址 */
    sizeof(states) / sizeof(states[0]),           /* 状态表元素个数 */
    transitions,                                  /* 转移表首地址 */
    sizeof(transitions) / sizeof(transitions[0]), /* 转移表元素个数 */
    state_names,                                  /* 状态名称表首地址 */
    sizeof(state_names) / sizeof(state_names[0]), /* 状态名称表元素个数 */
    event_names,                                  /* 事件名称表首地址 */
    sizeof(event_names) / sizeof(event_names[0]), /* 事件名称表元素个数 */
    STATE_IDLE                                    /* 初始状态 */
};

/* 全局互斥锁，保护状态机与用户上下文在“节拍线程”和“主线程”之间的访问 */
#ifdef _WIN32
static CRITICAL_SECTION g_lock; /* Windows 临界区，用于线程间互斥 */

/* 初始化互斥锁 */
static void lock_init(void)
{
    InitializeCriticalSection(&g_lock);
}

/* 销毁互斥锁 */
static void lock_destroy(void)
{
    DeleteCriticalSection(&g_lock);
}

/* 加锁 */
static void lock_acquire(void)
{
    EnterCriticalSection(&g_lock);
}

/* 解锁 */
static void lock_release(void)
{
    LeaveCriticalSection(&g_lock);
}
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER; /* POSIX 静态初始化互斥锁 */

/* 非 Windows 平台使用静态初始化的互斥锁，无需额外动作 */
static void lock_init(void)
{
}

/* 非 Windows 平台的互斥锁随进程结束自动释放 */
static void lock_destroy(void)
{
}

/* 加锁 */
static void lock_acquire(void)
{
    pthread_mutex_lock(&g_lock);
}

/* 解锁 */
static void lock_release(void)
{
    pthread_mutex_unlock(&g_lock);
}
#endif

/* 投递一个事件并立即处理，若当前状态不响应则给出提示 */
static void send_event(state_machine_t *machine, event_t event)
{
    lock_acquire();

    printf("[命令] %s\n", state_machine_event_name(machine, event));

    if (state_machine_post(machine, event) != 0) {
        printf("[警告] 事件队列已满，事件被丢弃\n");
    } else if (state_machine_process(machine) == 0) {
        printf("[提示] 当前状态 %s 不处理事件 %s\n",
               state_machine_state_name(machine, state_machine_current(machine)),
               state_machine_event_name(machine, event));
    }

    lock_release();
}

/* 连续执行指定个数的 500ms 节拍，供脚本模式使用 */
static void run_ticks(state_machine_t *machine, int ticks)
{
    int i;

    for (i = 0; i < ticks; ++i) {
        state_machine_run(machine);
        state_machine_process(machine);
    }
}

/* 脚本模式：不依赖真实定时器，按固定节拍跑完整流程，便于自动化验证 */
static void run_script(state_machine_t *machine)
{
    printf("[脚本] 第一阶段：start 后上升计数直到到达上限\n");
    send_event(machine, EVENT_START);
    run_ticks(machine, 11);

    printf("\n[脚本] 第二阶段：下降计数直至回到下限\n");
    run_ticks(machine, 10);

    printf("\n[脚本] 第三阶段：再次 start 后立刻 stop\n");
    send_event(machine, EVENT_START);
    run_ticks(machine, 3);
    send_event(machine, EVENT_STOP);
    run_ticks(machine, 1);
}

#ifdef _WIN32
/* 节拍线程控制块：由系统定时器周期性驱动状态机 */
typedef struct {
    state_machine_t *machine; /* 被驱动的状态机 */
    HANDLE thread;            /* 线程句柄 */
    HANDLE stop_event;        /* 停止信号，手动复位事件 */
    HANDLE timer;             /* 周期定时器，每 500ms 触发一次 */
} tick_thread_t;

/* 节拍线程入口：定时器到期后加锁执行状态机运行与事件处理 */
static DWORD WINAPI tick_thread_proc(LPVOID arg)
{
    tick_thread_t *tick = (tick_thread_t *)arg;
    HANDLE handles[2];

    handles[0] = tick->stop_event;
    handles[1] = tick->timer;

    for (;;) {
        DWORD result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        /* 只有定时器触发才执行节拍，收到停止信号或等待失败则退出 */
        if (result != WAIT_OBJECT_0 + 1) {
            break;
        }

        lock_acquire();
        state_machine_run(tick->machine);
        state_machine_process(tick->machine);
        lock_release();
    }

    return 0;
}

/* 停止节拍线程并释放线程、定时器和事件句柄 */
static void tick_thread_stop(tick_thread_t *tick)
{
    if (tick->stop_event != NULL) {
        SetEvent(tick->stop_event);
    }
    if (tick->thread != NULL) {
        WaitForSingleObject(tick->thread, INFINITE);
        CloseHandle(tick->thread);
        tick->thread = NULL;
    }
    if (tick->timer != NULL) {
        CloseHandle(tick->timer);
        tick->timer = NULL;
    }
    if (tick->stop_event != NULL) {
        CloseHandle(tick->stop_event);
        tick->stop_event = NULL;
    }
}

/* 启动节拍线程：创建周期定时器与工作线程，成功返回 0，失败返回 -1 */
static int tick_thread_start(tick_thread_t *tick, state_machine_t *machine, unsigned interval_ms)
{
    LARGE_INTEGER due;

    tick->machine = machine;
    tick->thread = NULL;
    tick->stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    tick->timer = CreateWaitableTimerA(NULL, FALSE, NULL);

    if (tick->stop_event == NULL || tick->timer == NULL) {
        tick_thread_stop(tick);
        return -1;
    }

    /* 负值表示相对时间，单位 100ns；同时给出周期实现自动重装 */
    due.QuadPart = -(LONGLONG)interval_ms * 10000;
    SetWaitableTimer(tick->timer, &due, (LONG)interval_ms, NULL, NULL, FALSE);

    tick->thread = CreateThread(NULL, 0, tick_thread_proc, tick, 0, NULL);
    if (tick->thread == NULL) {
        tick_thread_stop(tick);
        return -1;
    }

    return 0;
}

/* 查询控制台是否有按键输入，非阻塞 */
static int input_available(void)
{
    return _kbhit() != 0;
}

/* 主线程空闲等待，避免空转占满 CPU */
static void idle_wait(unsigned ms)
{
    Sleep(ms);
}
#else
/* 节拍线程控制块：由线程内部的周期延时驱动状态机 */
typedef struct {
    state_machine_t *machine; /* 被驱动的状态机 */
    pthread_t thread;         /* 线程句柄 */
    volatile int running;     /* 运行标志，置 0 后线程在下个周期退出 */
    unsigned interval_ms;     /* 节拍周期，单位 ms */
} tick_thread_t;

/* 节拍线程入口：按固定周期延时后加锁执行状态机运行与事件处理 */
static void *tick_thread_proc(void *arg)
{
    tick_thread_t *tick = (tick_thread_t *)arg;

    while (tick->running) {
        usleep(tick->interval_ms * 1000u);

        lock_acquire();
        state_machine_run(tick->machine);
        state_machine_process(tick->machine);
        lock_release();
    }

    return NULL;
}

/* 停止节拍线程并等待其退出 */
static void tick_thread_stop(tick_thread_t *tick)
{
    tick->running = 0;
    pthread_join(tick->thread, NULL);
}

/* 启动节拍线程，成功返回 0，失败返回 -1 */
static int tick_thread_start(tick_thread_t *tick, state_machine_t *machine, unsigned interval_ms)
{
    tick->machine = machine;
    tick->interval_ms = interval_ms;
    tick->running = 1;

    return (pthread_create(&tick->thread, NULL, tick_thread_proc, tick) == 0) ? 0 : -1;
}

/* 使用 select 非阻塞查询标准输入是否有数据 */
static int input_available(void)
{
    fd_set set;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    return select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0;
}

/* 主线程空闲等待，避免空转占满 CPU */
static void idle_wait(unsigned ms)
{
    usleep(ms * 1000u);
}
#endif

/* 交互模式：节拍线程每 500ms 驱动一次状态机，主线程只负责读取命令 */
static void run_interactive(state_machine_t *machine)
{
    tick_thread_t tick;
    int started;
    char line[64];

    started = (tick_thread_start(&tick, machine, TICK_INTERVAL_MS) == 0);
    if (!started) {
        printf("[警告] 节拍线程启动失败，计数不会自动推进\n");
    }

    printf("命令：start / stop / quit（每 500ms 自动计数一次）\n");

    for (;;) {
        /* 有输入时读取一整行命令，节拍线程在后台独立推进 */
        if (input_available()) {
            if (fgets(line, sizeof(line), stdin) == NULL) {
                break;
            }
            line[strcspn(line, "\r\n")] = '\0';

            if (strcmp(line, "start") == 0) {
                send_event(machine, EVENT_START);
            } else if (strcmp(line, "stop") == 0) {
                send_event(machine, EVENT_STOP);
            } else if (strcmp(line, "quit") == 0) {
                break;
            } else if (line[0] != '\0') {
                printf("[提示] 未知命令：%s\n", line);
            }
        }

        idle_wait(INPUT_POLL_MS);
    }

    /* 退出前先停掉节拍线程，保证后续统计不会与线程并发 */
    if (started) {
        tick_thread_stop(&tick);
    }
}

/* 演示入口：带 --script 参数时跑脚本流程，否则进入交互模式 */
int main(int argc, char **argv)
{
    static state_machine_t machine;  /* 状态机对象，静态分配避免占用栈空间 */
    counter_ctx_t ctx = { COUNT_MIN, 0 };

#ifdef _WIN32
    /* 源码与字面量均为 UTF-8，把控制台输入输出代码页切到 UTF-8，避免中文乱码 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    lock_init();
    state_machine_init(&machine, &config, &ctx, sizeof(ctx));

    if (argc > 1 && strcmp(argv[1], "--script") == 0) {
        run_script(&machine);
    } else {
        run_interactive(&machine);
    }

    printf("\n结束：状态=%s 计数=%d 节拍=%d\n",
           state_machine_state_name(&machine, state_machine_current(&machine)),
           ctx.count, ctx.ticks);

    lock_destroy();
    return 0;
}
