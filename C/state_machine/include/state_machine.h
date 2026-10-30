#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stddef.h>

/* 状态机对象类型的前向声明，完整定义见下方 struct state_machine */
typedef struct state_machine state_machine_t;

/* 状态编号类型，取值由使用者自定义（负数保留为特殊值） */
typedef int state_t;

/* 事件编号类型，取值由使用者自定义 */
typedef int event_t;

/* 特殊状态值，表示本次转移不改变状态，仅执行转移动作 */
#define STATE_NONE (-1)

/* 事件队列容量，即最多可缓存但尚未处理的待处理事件个数 */
#define STATE_MACHINE_EVENT_QUEUE_SIZE 16

/* 状态机回调函数类型，进入、退出、运行以及转移动作均使用该签名 */
typedef void (*state_machine_action_t)(state_machine_t *machine);

/* 状态表元素，描述一个状态编号以及进入、退出、运行三个回调 */
typedef struct {
    state_t id;                      /* 状态编号，在状态表中需唯一 */
    state_machine_action_t on_enter; /* 进入状态时执行的回调，可为 NULL */
    state_machine_action_t on_exit;  /* 退出状态时执行的回调，可为 NULL */
    state_machine_action_t on_run;   /* 状态运行期间每周期执行的回调，可为 NULL */
} state_machine_state_t;

/* 转移表元素，描述“在某个状态收到某个事件后做什么” */
typedef struct {
    state_t current;               /* 触发事件时所处的当前状态编号 */
    event_t event;                 /* 触发本行规则的事件编号 */
    state_t next;                  /* 转移到的下一状态编号，STATE_NONE 表示不转移 */
    state_machine_action_t action; /* 转移时执行的动作回调，可为 NULL */
} state_machine_transition_t;

/* 状态机静态配置，通常定义为 const 全局变量，被状态机长期引用 */
typedef struct {
    const state_machine_state_t *states;           /* 状态表首地址 */
    size_t state_count;                            /* 状态表元素个数 */
    const state_machine_transition_t *transitions; /* 转移表首地址 */
    size_t transition_count;                       /* 转移表元素个数 */
    const char *const *state_names;                /* 状态名称数组，下标即状态编号，仅用于打印，可为 NULL */
    size_t state_name_count;                       /* 状态名称数组元素个数 */
    const char *const *event_names;                /* 事件名称数组，下标即事件编号，仅用于打印，可为 NULL */
    size_t event_count;                            /* 事件名称数组元素个数 */
    state_t initial;                               /* 状态机初始化后进入的初始状态编号 */
} state_machine_config_t;

/* 状态机运行时对象，由使用者提供存储空间，内部字段全部由库维护 */
struct state_machine {
    const state_machine_config_t *cfg;             /* 指向静态配置，生命周期需长于状态机 */
    state_t current;                               /* 当前所处的状态编号 */
    void *user;                                    /* 用户自定义上下文指针，库只保存不修改 */
    size_t user_size;                              /* 用户上下文字节数，供调用方校验类型是否匹配 */
    event_t queue[STATE_MACHINE_EVENT_QUEUE_SIZE]; /* 事件环形队列缓冲区 */
    size_t head;                                   /* 环形队列头下标，即下一个出队位置 */
    size_t tail;                                   /* 环形队列尾下标，即下一个入队位置 */
    size_t count;                                  /* 环形队列中当前缓存的事件个数 */
};

/* 初始化状态机：绑定配置与用户数据，清空事件队列并进入初始状态。
 * user_size 为用户上下文的结构体字节数，库只保存，不会越界访问；
 * 传 0 或不传用户数据时表示不使用上下文。
 * 进入初始状态时会调用该状态的 on_enter 回调。 */
void state_machine_init(state_machine_t *machine,
                        const state_machine_config_t *cfg,
                        void *user,
                        size_t user_size);

/* 投递事件到事件队列，只入队不立即处理。
 * 返回 0 表示投递成功，返回 -1 表示参数非法或队列已满。 */
int state_machine_post(state_machine_t *machine, event_t event);

/* 立即处理一个事件：查转移表，执行转移动作，并在需要时退出旧状态、进入新状态。
 * 返回 0 表示事件被处理，返回 -1 表示参数非法或当前状态对事件无匹配规则。 */
int state_machine_dispatch(state_machine_t *machine, event_t event);

/* 依次取出事件队列中的所有事件并逐个分发处理，返回实际被处理的事件个数。 */
int state_machine_process(state_machine_t *machine);

/* 执行一次当前状态的运行回调 on_run，通常放在主循环或定时任务中周期调用。 */
void state_machine_run(state_machine_t *machine);

/* 获取当前状态编号，若状态机指针为 NULL 则返回 STATE_NONE。 */
state_t state_machine_current(const state_machine_t *machine);

/* 获取初始化时绑定的用户数据指针，若状态机指针为 NULL 则返回 NULL。 */
void *state_machine_user(const state_machine_t *machine);

/* 获取初始化时登记的用户上下文字节数，未使用上下文时返回 0。 */
size_t state_machine_user_size(const state_machine_t *machine);

/* 根据状态编号在状态名称表中查找名称，未提供名称表或编号越界时返回 "?"。 */
const char *state_machine_state_name(const state_machine_t *machine, state_t state);

/* 根据事件编号查找事件名称，找不到时返回 "?"。 */
const char *state_machine_event_name(const state_machine_t *machine, event_t event);

#endif
