#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "state_machine.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <time.h>
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

/* 投递一个事件并立即处理，若当前状态不响应则给出提示 */
static void send_event(state_machine_t *machine, event_t event)
{
    printf("[命令] %s\n", state_machine_event_name(machine, event));

    if (state_machine_post(machine, event) != 0) {
        printf("[警告] 事件队列已满，事件被丢弃\n");
        return;
    }

    if (state_machine_process(machine) == 0) {
        printf("[提示] 当前状态 %s 不处理事件 %s\n",
               state_machine_state_name(machine, state_machine_current(machine)),
               state_machine_event_name(machine, event));
    }
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
/* Windows 定时器句柄，用于产生 500ms 节拍延时 */
typedef struct {
    HANDLE handle;          /* 可等待定时器句柄，创建失败时为 NULL */
    unsigned interval_ms;   /* 定时周期，单位 ms */
    unsigned long long last_ms; /* 创建失败时的软件计时基准，单位 ms */
} demo_timer_t;

/* 创建并启动周期定时器；创建失败则退化为基于 GetTickCount64 的软件计时 */
static void timer_start(demo_timer_t *timer, unsigned interval_ms)
{
    LARGE_INTEGER due;

    timer->interval_ms = interval_ms;
    timer->handle = CreateWaitableTimerA(NULL, FALSE, NULL);
    if (timer->handle == NULL) {
        timer->last_ms = GetTickCount64();
        printf("[警告] 创建定时器失败，改用软件计时\n");
        return;
    }

    /* 负值表示相对时间，单位 100ns；同时给出周期实现自动重装 */
    due.QuadPart = -(LONGLONG)interval_ms * 10000;
    SetWaitableTimer(timer->handle, &due, (LONG)interval_ms, NULL, NULL, FALSE);
}

/* 等待定时器或超时：返回 1 表示一个周期已到，返回 0 表示仅超时返回 */
static int timer_wait(demo_timer_t *timer, unsigned poll_ms)
{
    unsigned long long now;

    if (timer->handle == NULL) {
        Sleep(poll_ms);
        now = GetTickCount64();
        if (now - timer->last_ms >= timer->interval_ms) {
            timer->last_ms += timer->interval_ms;
            return 1;
        }
        return 0;
    }

    return WaitForSingleObject(timer->handle, poll_ms) == WAIT_OBJECT_0;
}

/* 关闭并释放定时器资源 */
static void timer_stop(demo_timer_t *timer)
{
    if (timer->handle != NULL) {
        CloseHandle(timer->handle);
        timer->handle = NULL;
    }
}

/* 查询控制台是否有按键输入，非阻塞 */
static int input_available(void)
{
    return _kbhit() != 0;
}
#else
/* 非 Windows 平台使用单调时钟模拟周期定时器 */
typedef struct {
    unsigned interval_ms;       /* 定时周期，单位 ms */
    unsigned long long last_ms; /* 上一次节拍的时间戳，单位 ms */
} demo_timer_t;

/* 读取单调时钟，返回毫秒数 */
static unsigned long long monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000u
         + (unsigned long long)ts.tv_nsec / 1000000u;
}

/* 记录起始时间并保存定时周期 */
static void timer_start(demo_timer_t *timer, unsigned interval_ms)
{
    timer->interval_ms = interval_ms;
    timer->last_ms = monotonic_ms();
}

/* 延时 poll_ms 后检查是否已到一个周期：返回 1 表示周期到，返回 0 表示未到 */
static int timer_wait(demo_timer_t *timer, unsigned poll_ms)
{
    unsigned long long now;

    usleep(poll_ms * 1000u);
    now = monotonic_ms();
    if (now - timer->last_ms >= timer->interval_ms) {
        timer->last_ms += timer->interval_ms;
        return 1;
    }
    return 0;
}

/* 非 Windows 平台无需释放定时器资源 */
static void timer_stop(demo_timer_t *timer)
{
    (void)timer;
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
#endif

/* 交互模式：定时器每 500ms 触发一次节拍，同时随时响应键盘命令 */
static void run_interactive(state_machine_t *machine)
{
    demo_timer_t timer;
    char line[64];

    timer_start(&timer, TICK_INTERVAL_MS);
    printf("命令：start / stop / quit（每 500ms 自动计数一次）\n");

    for (;;) {
        /* 有输入时读取一整行命令，命令之间按键不会影响节拍 */
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

        /* 定时器到期就执行一次状态运行与事件处理 */
        if (timer_wait(&timer, INPUT_POLL_MS)) {
            state_machine_run(machine);
            state_machine_process(machine);
        }
    }

    timer_stop(&timer);
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

    state_machine_init(&machine, &config, &ctx, sizeof(ctx));

    if (argc > 1 && strcmp(argv[1], "--script") == 0) {
        run_script(&machine);
    } else {
        run_interactive(&machine);
    }

    printf("\n结束：状态=%s 计数=%d 节拍=%d\n",
           state_machine_state_name(&machine, state_machine_current(&machine)),
           ctx.count, ctx.ticks);

    return 0;
}
