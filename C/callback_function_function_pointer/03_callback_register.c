/*
 * 案例 3：把函数指针作为参数，用来注册回调函数
 *
 * 与案例 2 的区别：
 *   案例 2 是“立刻使用”回调；
 *   案例 3 是“先保存”回调（注册），等事件真正发生时再由模块调用。
 */

#include <stdio.h>

#include "console_utf8.h"

/* 1. 定义回调类型（参数：事件编号 + 用户上下文） */
typedef void (*ButtonCallback)(int button_id, void *user_data);

/* 2. 被注册回调的模块（相当于驱动 / 事件源） */
typedef struct {
    ButtonCallback callback; /* 保存注册进来的函数指针 */
    void *user_data;         /* 回调时要携带的用户数据 */
} Button;

static void button_init(Button *btn) {
    btn->callback = NULL;
    btn->user_data = NULL;
}

/* 3. 注册函数：函数指针作为参数传入，保存到模块内部 */
static void button_register_callback(Button *btn, ButtonCallback callback, void *user_data) {
    btn->callback = callback;
    btn->user_data = user_data;
}

/* 4. 事件发生点：统一在这里调用已注册的回调 */
static void button_press(Button *btn, int button_id) {
    printf("[驱动] 按键 %d 被按下\n", button_id);
    if (btn->callback != NULL) {
        btn->callback(button_id, btn->user_data); /* 回调用户代码 */
    }
}

/* ---------- 用户侧的回调实现 ---------- */

static void on_led_toggle(int button_id, void *user_data) {
    int *led_state = (int *)user_data;
    *led_state = !(*led_state);
    printf("[用户] 按键 %d -> LED = %s\n", button_id, (*led_state != 0) ? "ON" : "OFF");
}

static void on_print_message(int button_id, void *user_data) {
    const char *message = (const char *)user_data;
    printf("[用户] 按键 %d -> %s\n", button_id, message);
}

int main(void) {
    console_utf8_init();

    Button button;
    int led_state = 0;

    button_init(&button);

    /* 未注册回调：事件仍可发生，只是没有用户代码被调用 */
    button_press(&button, 1);

    /* 注册回调 1 */
    button_register_callback(&button, on_led_toggle, &led_state);
    button_press(&button, 1);
    button_press(&button, 1);

    /* 重新注册：直接换成另一个回调 */
    button_register_callback(&button, on_print_message, (void *)"hello callback");
    button_press(&button, 2);

    /* 传 NULL 取消注册 */
    button_register_callback(&button, NULL, NULL);
    button_press(&button, 3);

    return 0;
}
