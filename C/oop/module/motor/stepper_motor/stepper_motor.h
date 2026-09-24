#ifndef MODULE_STEPPER_MOTOR_H
#define MODULE_STEPPER_MOTOR_H

#include "port/motor/motor.h"
#include "bsp/bsp_gpio/bsp_gpio.h"
#include <assert.h>

/*
 * 步进电机：继承 Motor，填写自己的 ops 表。
 *  - 故意把 base 放在【非首位】，用来证明 container_of 不依赖“首成员”。
 *  - 填写了全部必填槽位。
 *  - 填写了 calibrate（可选槽位）。
 *  - 故意不填 brake（可选槽位，置 NULL）→ 调用会 assert 崩溃。
 */
typedef struct StepperMotor {
    bsp_pin_t step_pin;
    bsp_pin_t dir_pin;
    Motor     base;       /* 继承：故意放在非首位，向下转型必须用 container_of */
} StepperMotor;

void stepper_motor_init(StepperMotor* self, bsp_pin_t step_pin,
                        bsp_pin_t dir_pin);

/* 向下转型：基类指针 -> StepperMotor 指针（container_of + 类型检查，失败则 assert 崩溃） */
static inline StepperMotor* motor_as_stepper(Motor* m) {
    assert(m != NULL);
    assert(m->kind == MOTOR_STEPPER && "motor 不是 StepperMotor，向下转型失败");
    return container_of(m, StepperMotor, base);
}

#endif /* MODULE_STEPPER_MOTOR_H */
