#include <assert.h>
#include <stdio.h>

#include "state_machine.h"

/* 演示用的状态编号 */
enum {
    STATE_IDLE,    /* 空闲：等待启动命令 */
    STATE_RUNNING, /* 运行：正在处理业务 */
    STATE_PAUSED,  /* 暂停：保持上下文但停止处理 */
    STATE_DONE     /* 完成：等待复位重新开始 */
};

/* 演示用的事件编号，需与 event_names 数组下标一一对应 */
enum {
    EVENT_START,  /* 启动 */
    EVENT_PAUSE,  /* 暂停 */
    EVENT_RESUME, /* 继续 */
    EVENT_STOP,   /* 停止 */
    EVENT_RESET   /* 复位 */
};

/* 状态名称字符串表，下标与状态编号一一对应，仅用于日志打印 */
static const char *const state_names[] = {
    "IDLE", "RUNNING", "PAUSED", "DONE"
};

/* 事件名称字符串表，下标与事件编号一一对应，仅用于日志打印 */
static const char *const event_names[] = {
    "START", "PAUSE", "RESUME", "STOP", "RESET"
};

/* 演示用的用户上下文，挂在状态机上供各回调共享计数 */
typedef struct {
    int ticks;    /* 运行回调被调用的总次数 */
    int enters;   /* 进入状态的总次数 */
    int exits;    /* 退出状态的总次数 */
    int rejected; /* 被忽略的事件次数 */
} demo_ctx_t;

/* 从状态机取出用户上下文并转换为 demo_ctx_t 指针。
 * 先校验上下文大小，防止类型不匹配导致越界访问。 */
static demo_ctx_t *ctx_of(state_machine_t *machine)
{
    assert(state_machine_user_size(machine) == sizeof(demo_ctx_t));
    return (demo_ctx_t *)state_machine_user(machine);
}

/* IDLE 状态的进入回调：累计进入次数并打印日志 */
static void idle_enter(state_machine_t *machine)
{
    ctx_of(machine)->enters++;
    printf("  [enter] IDLE\n");
}

/* IDLE 状态的退出回调：累计退出次数并打印日志 */
static void idle_exit(state_machine_t *machine)
{
    ctx_of(machine)->exits++;
    printf("  [exit ] IDLE (exits=%d)\n", ctx_of(machine)->exits);
}

/* IDLE 状态的周期回调：累计运行次数并打印等待提示 */
static void idle_run(state_machine_t *machine)
{
    ctx_of(machine)->ticks++;
    printf("  [run  ] IDLE waiting for START\n");
}

/* RUNNING 状态的进入回调：累计进入次数并打印日志 */
static void running_enter(state_machine_t *machine)
{
    ctx_of(machine)->enters++;
    printf("  [enter] RUNNING\n");
}

/* RUNNING 状态的退出回调：累计退出次数并打印日志 */
static void running_exit(state_machine_t *machine)
{
    ctx_of(machine)->exits++;
    printf("  [exit ] RUNNING (exits=%d)\n", ctx_of(machine)->exits);
}

/* RUNNING 状态的周期回调：累计运行次数并打印当前节拍 */
static void running_run(state_machine_t *machine)
{
    ctx_of(machine)->ticks++;
    printf("  [run  ] RUNNING processing tick %d\n", ctx_of(machine)->ticks);
}

/* PAUSED 状态的周期回调：累计运行次数并打印挂起提示 */
static void paused_run(state_machine_t *machine)
{
    ctx_of(machine)->ticks++;
    printf("  [run  ] PAUSED suspended\n");
}

/* DONE 状态的进入回调：累计进入次数并打印日志 */
static void done_enter(state_machine_t *machine)
{
    ctx_of(machine)->enters++;
    printf("  [enter] DONE\n");
}

/* DONE 状态的退出回调：累计退出次数并打印日志 */
static void done_exit(state_machine_t *machine)
{
    ctx_of(machine)->exits++;
    printf("  [exit ] DONE (exits=%d)\n", ctx_of(machine)->exits);
}

/* 打印一条转移动作日志，附带动作发生时所在的状态 */
static void log_transition(state_machine_t *machine, const char *what)
{
    printf("  <act  > %s from %s\n", what,
           state_machine_state_name(machine, state_machine_current(machine)));
}

/* EVENT_START 的转移动作：记录启动请求 */
static void act_start(state_machine_t *machine)
{
    log_transition(machine, "start requested");
}

/* EVENT_PAUSE 的转移动作：记录暂停请求 */
static void act_pause(state_machine_t *machine)
{
    log_transition(machine, "pause requested");
}

/* EVENT_RESUME 的转移动作：记录继续请求 */
static void act_resume(state_machine_t *machine)
{
    log_transition(machine, "resume requested");
}

/* EVENT_STOP 的转移动作：记录停止请求 */
static void act_stop(state_machine_t *machine)
{
    log_transition(machine, "stop requested");
}

/* EVENT_RESET 的转移动作：记录复位请求 */
static void act_reset(state_machine_t *machine)
{
    log_transition(machine, "reset requested");
}

/* 非法事件的处理动作：不转移状态，只统计并提示被忽略 */
static void act_ignore_stop(state_machine_t *machine)
{
    ctx_of(machine)->rejected++;
    printf("  <act  > STOP ignored in %s\n",
           state_machine_state_name(machine, state_machine_current(machine)));
}

/* 状态表：每行依次为 状态编号、进入回调、退出回调、运行回调。
 * 回调槽位填 NULL 表示该状态不需要这个回调，库会自动跳过。 */
static const state_machine_state_t states[] = {
    { STATE_IDLE,    idle_enter,    idle_exit,    idle_run    },
    { STATE_RUNNING, running_enter, running_exit, running_run },
    { STATE_PAUSED,  NULL,          NULL,         paused_run  }, /* 无进入、退出回调 */
    { STATE_DONE,    done_enter,    done_exit,    NULL        }  /* 无运行回调 */
};

/* 转移表：每行依次为 当前状态、触发事件、下一状态、转移动作。
 * 下一状态填 STATE_NONE 表示只执行动作而不改变状态；
 * 转移动作填 NULL 表示只切换状态、不执行动作。 */
static const state_machine_transition_t transitions[] = {
    { STATE_IDLE,    EVENT_START,  STATE_RUNNING, act_start },
    { STATE_IDLE,    EVENT_STOP,   STATE_NONE,    act_ignore_stop },
    { STATE_RUNNING, EVENT_PAUSE,  STATE_PAUSED,  act_pause },
    { STATE_RUNNING, EVENT_STOP,   STATE_DONE,    act_stop },
    { STATE_RUNNING, EVENT_START,  STATE_NONE,    act_start },
    { STATE_PAUSED,  EVENT_RESUME, STATE_RUNNING, act_resume },
    { STATE_PAUSED,  EVENT_STOP,   STATE_DONE,    NULL }, /* 无转移动作 */
    { STATE_DONE,    EVENT_RESET,  STATE_IDLE,    act_reset }
};

/* 状态机静态配置：状态表、转移表、名称表以及初始状态 */
static const state_machine_config_t config = {
    states,                                    /* 状态表首地址 */
    sizeof(states) / sizeof(states[0]),        /* 状态表元素个数 */
    transitions,                               /* 转移表首地址 */
    sizeof(transitions) / sizeof(transitions[0]), /* 转移表元素个数 */
    state_names,                               /* 状态名称表首地址 */
    sizeof(state_names) / sizeof(state_names[0]), /* 状态名称表元素个数 */
    event_names,                               /* 事件名称表首地址 */
    sizeof(event_names) / sizeof(event_names[0]), /* 事件名称表元素个数 */
    STATE_IDLE                                 /* 初始状态 */
};

/* 模拟一个调度周期：先运行当前状态，再处理事件队列，最后打印新状态 */
static void step(state_machine_t *machine, const char *label)
{
    printf("== %s (state: %s)\n", label,
           state_machine_state_name(machine, state_machine_current(machine)));
    state_machine_run(machine);
    state_machine_process(machine);
    printf("-- now: %s\n\n",
           state_machine_state_name(machine, state_machine_current(machine)));
}

/* 投递一个事件并打印事件名称，便于观察演示过程 */
static void fire(state_machine_t *machine, event_t event)
{
    printf(">> post %s\n", state_machine_event_name(machine, event));
    state_machine_post(machine, event);
}

/* 演示入口：初始化状态机并依次模拟若干调度周期 */
int main(void)
{
    static state_machine_t machine;      /* 状态机对象，静态分配避免占用栈空间 */
    demo_ctx_t ctx = { 0, 0, 0, 0 };     /* 用户上下文，保存运行统计 */

    state_machine_init(&machine, &config, &ctx, sizeof(ctx));
    printf("init state: %s\n\n",
           state_machine_state_name(&machine, state_machine_current(&machine)));

    fire(&machine, EVENT_STOP);   /* 空闲状态下停止：动作执行但状态不变 */
    step(&machine, "tick 1");

    fire(&machine, EVENT_START);  /* 空闲 -> 运行 */
    step(&machine, "tick 2");

    fire(&machine, EVENT_START);  /* 运行中重复启动：仅执行动作 */
    fire(&machine, EVENT_PAUSE);  /* 运行 -> 暂停 */
    step(&machine, "tick 3");

    fire(&machine, EVENT_RESUME); /* 暂停 -> 运行 */
    step(&machine, "tick 4");

    fire(&machine, EVENT_STOP);   /* 运行 -> 完成 */
    step(&machine, "tick 5");

    fire(&machine, EVENT_RESET);  /* 完成 -> 空闲 */
    step(&machine, "tick 6");

    printf("summary: ticks=%d enters=%d exits=%d rejected=%d\n",
           ctx.ticks, ctx.enters, ctx.exits, ctx.rejected);

    return 0;
}
