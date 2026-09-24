#ifndef PORT_UI_UI_H
#define PORT_UI_UI_H

#include <stddef.h>
#include <stdbool.h>

/*
 * 接口层：Ui “基类”（人机交互接口）。
 * 按“最终 GUI 的控件”设计：一个【电机开关】+ 一个【转速滑条】+ 一个【实际转速显示】。
 *
 * 采用“回调（事件驱动）”模型：
 *   1. 应用先 ui_set_handler 注册事件回调；
 *   2. 应用调用 ui_run，UI 拥有主循环；
 *   3. 用户操作时，UI 通过回调把事件推给应用；
 *   4. 应用通过 ui_show_* 把状态回推给 UI 显示。
 * 命令行（console_ui）和 LVGL（lvgl_ui）都是同一张 ops 表的实现。
 */

typedef struct Ui Ui;
typedef struct UiOps UiOps;

/* UI 事件：用户对控件的操作 */
typedef enum {
    UI_EV_NONE = 0,
    UI_EV_SWITCH_ON,   /* 开关拨到 ON */
    UI_EV_SWITCH_OFF,  /* 开关拨到 OFF */
    UI_EV_SPEED,       /* 滑条变化，值在 value */
    UI_EV_QUIT,        /* 退出 */
} UiEvent;

/* 事件回调：UI 收到用户操作时调用。user_data 是 ui_set_handler 时传入的上下文 */
typedef void (*UiEventHandler)(void* user_data, UiEvent event, int value);

/* ops 表（vtable）：这张表就是“契约”，每个实现都必须填写 */
struct UiOps {
    void (*show_switch)(Ui* self, bool on);          /* 回显开关状态 */
    void (*show_speed)(Ui* self, int value);         /* 回显【设定】转速（滑条） */
    void (*show_actual_speed)(Ui* self, int value);  /* 回显【实际】转速（数值显示）*/
    void (*run)(Ui* self);                           /* UI 主循环，事件经回调推送 */
};

struct Ui {
    const UiOps*    ops;        /* vtable 指针 */
    UiEventHandler handler;     /* 事件回调 */
    void*           user_data;  /* 回调上下文 */
};

/* “虚函数”分发接口（内部先 assert 检查 ops 槽位是否已填写） */
void ui_show_switch(Ui* self, bool on);
void ui_show_speed(Ui* self, int value);
void ui_show_actual_speed(Ui* self, int value);
void ui_run(Ui* self);

/* 注册事件回调（通用，不依赖具体 UI） */
void ui_set_handler(Ui* self, UiEventHandler handler, void* user_data);

#endif /* PORT_UI_UI_H */
