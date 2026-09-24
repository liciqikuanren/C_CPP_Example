#include "app/motor_ui/motor_ui.h"
#include "port/ui/ui.h"
#include "port/motor/motor.h"
#include <assert.h>
#include <stdbool.h>

/* 回调上下文：事件回调需要同时访问 UI 和电机 */
typedef struct {
    Ui*    ui;
    Motor* motor;
    bool   running;   /* 电机是否在运行（决定“实际转速”） */
} MotorUiCtx;

/* 把【实际转速】推给 UI：停机时为 0，运行时为电机当前速度 */
static void push_actual_speed(MotorUiCtx* ctx) {
    int actual = ctx->running ? (int)motor_speed(ctx->motor) : 0;
    ui_show_actual_speed(ctx->ui, actual);
}

/* 事件回调：用户操作 → 驱动电机 + 回显状态 */
static void on_event(void* user_data, UiEvent event, int value) {
    MotorUiCtx* ctx = (MotorUiCtx*)user_data;

    switch (event) {
    case UI_EV_SWITCH_ON:
        motor_start(ctx->motor);
        ctx->running = true;
        ui_show_switch(ctx->ui, true);
        push_actual_speed(ctx);
        break;

    case UI_EV_SWITCH_OFF:
        motor_stop(ctx->motor);
        ctx->running = false;
        ui_show_switch(ctx->ui, false);
        push_actual_speed(ctx);             /* 停机 → 实际转速归 0 */
        break;

    case UI_EV_SPEED:
        motor_set_speed(ctx->motor, (float)value);
        ui_show_speed(ctx->ui, value);      /* 设定值 → 滑条 */
        push_actual_speed(ctx);             /* 实际值 → 数值显示 */
        break;

    case UI_EV_QUIT:
        motor_stop(ctx->motor);
        break;

    case UI_EV_NONE:
    default:
        break;
    }
}

void motor_ui_run(Ui* ui, Motor* motor) {
    MotorUiCtx ctx;

    assert(ui != NULL);
    assert(motor != NULL);

    ctx.ui      = ui;
    ctx.motor   = motor;
    ctx.running = false;

    /* 初始状态：停止、设定 0、实际 0 */
    ui_show_switch(ui, false);
    ui_show_speed(ui, 0);
    ui_show_actual_speed(ui, 0);

    /* 注册回调，把主循环交给 UI */
    ui_set_handler(ui, on_event, &ctx);
    ui_run(ui);
}
