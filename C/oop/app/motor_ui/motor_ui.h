#ifndef APP_MOTOR_UI_H
#define APP_MOTOR_UI_H

#include "port/ui/ui.h"
#include "port/motor/motor.h"

/*
 * 应用层：电机控制面板逻辑（屏幕无关）。
 * 采用回调模型：注册事件回调后，把主循环交给 UI（ui_run）。
 * 不依赖具体 UI 实现，也不感知具体电机类型。
 */

/* 在指定 UI 上运行控制面板，控制指定电机 */
void motor_ui_run(Ui* ui, Motor* motor);

#endif /* APP_MOTOR_UI_H */
