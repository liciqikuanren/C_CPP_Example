#include "state_machine.h"

/* 在静态配置的状态表中按编号查找状态描述，未找到返回 NULL */
static const state_machine_state_t *find_state(const state_machine_t *machine, state_t id)
{
    size_t i;

    for (i = 0; i < machine->cfg->state_count; ++i) {
        if (machine->cfg->states[i].id == id) {
            return &machine->cfg->states[i];
        }
    }
    return NULL;
}

/* 在静态配置的转移表中查找匹配“当前状态 + 事件”的规则，未找到返回 NULL */
static const state_machine_transition_t *find_transition(const state_machine_t *machine,
                                                         state_t current, event_t event)
{
    size_t i;

    for (i = 0; i < machine->cfg->transition_count; ++i) {
        const state_machine_transition_t *t = &machine->cfg->transitions[i];

        if (t->current == current && t->event == event) {
            return t;
        }
    }
    return NULL;
}

/* 内部辅助函数：切换当前状态到指定编号，并调用新状态的进入回调 on_enter */
static void enter_state(state_machine_t *machine, state_t id, const state_machine_state_t *st)
{
    machine->current = id;
    if (st != NULL && st->on_enter != NULL) {
        st->on_enter(machine);
    }
}

/* 初始化状态机：保存配置和用户数据，清空事件队列，进入初始状态 */
void state_machine_init(state_machine_t *machine, const state_machine_config_t *cfg,
                        void *user, size_t user_size)
{
    if (machine == NULL || cfg == NULL) {
        return;
    }

    machine->cfg = cfg;
    machine->user = user;
    machine->user_size = (user != NULL) ? user_size : 0;
    machine->current = STATE_NONE;
    machine->head = 0;
    machine->tail = 0;
    machine->count = 0;

    /* 进入初始状态，触发其 on_enter 回调 */
    enter_state(machine, cfg->initial, find_state(machine, cfg->initial));
}

/* 将事件写入环形队列；队列满或参数非法时返回 -1，成功返回 0 */
int state_machine_post(state_machine_t *machine, event_t event)
{
    if (machine == NULL || machine->cfg == NULL) {
        return -1;
    }
    if (machine->count >= STATE_MACHINE_EVENT_QUEUE_SIZE) {
        return -1;
    }

    machine->queue[machine->tail] = event;
    machine->tail = (machine->tail + 1) % STATE_MACHINE_EVENT_QUEUE_SIZE;
    machine->count++;
    return 0;
}

/* 分发单个事件：执行转移动作，必要时先退出旧状态再进入新状态 */
int state_machine_dispatch(state_machine_t *machine, event_t event)
{
    const state_machine_state_t *cur;
    const state_machine_state_t *next;
    const state_machine_transition_t *t;

    if (machine == NULL || machine->cfg == NULL) {
        return -1;
    }

    /* 查表：当前状态与事件都没有匹配规则时，忽略该事件 */
    t = find_transition(machine, machine->current, event);
    if (t == NULL) {
        return -1;
    }

    /* 先执行转移动作，此时状态尚未切换 */
    if (t->action != NULL) {
        t->action(machine);
    }

    /* next 为 STATE_NONE 表示只执行动作，不改变状态 */
    if (t->next != STATE_NONE && t->next != machine->current) {
        cur = find_state(machine, machine->current);
        if (cur != NULL && cur->on_exit != NULL) {
            cur->on_exit(machine);
        }

        next = find_state(machine, t->next);
        if (next == NULL) {
            return -1;
        }
        enter_state(machine, next->id, next);
    }

    return 0;
}

/* 逐个取出队列中的事件并分发，返回被成功处理的事件个数 */
int state_machine_process(state_machine_t *machine)
{
    int processed = 0;

    if (machine == NULL || machine->cfg == NULL) {
        return 0;
    }

    while (machine->count > 0) {
        event_t event = machine->queue[machine->head];

        machine->head = (machine->head + 1) % STATE_MACHINE_EVENT_QUEUE_SIZE;
        machine->count--;

        if (state_machine_dispatch(machine, event) == 0) {
            processed++;
        }
    }

    return processed;
}

/* 运行当前状态的周期回调 on_run */
void state_machine_run(state_machine_t *machine)
{
    const state_machine_state_t *cur;

    if (machine == NULL || machine->cfg == NULL) {
        return;
    }

    cur = find_state(machine, machine->current);
    if (cur != NULL && cur->on_run != NULL) {
        cur->on_run(machine);
    }
}

/* 读取当前状态编号 */
state_t state_machine_current(const state_machine_t *machine)
{
    return (machine != NULL) ? machine->current : STATE_NONE;
}

/* 读取用户数据指针 */
void *state_machine_user(const state_machine_t *machine)
{
    return (machine != NULL) ? machine->user : NULL;
}

/* 读取用户上下文字节数 */
size_t state_machine_user_size(const state_machine_t *machine)
{
    return (machine != NULL) ? machine->user_size : 0;
}

/* 按编号在状态名称表中查找名称，查不到时返回占位字符串 "?" */
const char *state_machine_state_name(const state_machine_t *machine, state_t state)
{
    const char *name;

    if (machine == NULL || machine->cfg == NULL || machine->cfg->state_names == NULL) {
        return "?";
    }
    if (state < 0 || (size_t)state >= machine->cfg->state_name_count) {
        return "?";
    }

    name = machine->cfg->state_names[state];
    return (name != NULL) ? name : "?";
}

/* 按编号查找事件名称，查不到时返回占位字符串 "?" */
const char *state_machine_event_name(const state_machine_t *machine, event_t event)
{
    if (machine == NULL || machine->cfg == NULL || machine->cfg->event_names == NULL) {
        return "?";
    }
    if (event < 0 || (size_t)event >= machine->cfg->event_count) {
        return "?";
    }
    return machine->cfg->event_names[event];
}
