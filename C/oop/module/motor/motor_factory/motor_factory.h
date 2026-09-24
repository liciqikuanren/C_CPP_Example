#ifndef MODULE_MOTOR_FACTORY_H
#define MODULE_MOTOR_FACTORY_H

#include "port/motor/motor.h"

/*
 * 简单工厂 + 扩展点（开闭原则）。
 * 新增一种电机（例如 ServoMotor）时：
 *   1) 新建 ServoMotor 结构体 + 自己的 ops 表；
 *   2) 在 motor_create() 里加一个分支。
 * 除此之外，既有结构体、ops 表、应用层主流程都无需改动。
 * 这就是“对扩展开放，对修改关闭”。
 */
Motor* motor_create(const char* type);
void  motor_destroy(Motor* motor);

#endif /* MODULE_MOTOR_FACTORY_H */
