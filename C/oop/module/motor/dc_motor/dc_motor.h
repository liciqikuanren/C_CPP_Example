#ifndef MODULE_DC_MOTOR_H
#define MODULE_DC_MOTOR_H

#include "port/motor/motor.h"
#include "bsp/bsp_gpio/bsp_gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/*
 * 直流电机：继承 Motor（base 放在第一个成员），并填写自己的 ops 表。
 *  - 填写了全部必填槽位。
 *  - 填写了 brake（可选槽位）。
 *  - 故意不填 calibrate（可选槽位，置 NULL）→ 调用会 assert 崩溃。
 */
typedef struct DcMotor {
    Motor     base;        /* 继承：内嵌的“基类”成员（本例仍放在首位，offsetof==0） */
    uint8_t   pwm_channel;
    bsp_pin_t en_pin;
    bsp_pin_t dir_pin;
    bool      running;     /* 封装：内部状态，外部不要直接改 */
} DcMotor;

/* 构造函数 */
void dc_motor_init(DcMotor* self, uint8_t pwm_channel,
                   bsp_pin_t en_pin, bsp_pin_t dir_pin);

/* 向下转型：基类指针 -> DcMotor 指针（container_of + 类型检查，失败则 assert 崩溃） */
static inline DcMotor* motor_as_dc(Motor* m) {
    assert(m != NULL);
    assert(m->kind == MOTOR_DC && "motor 不是 DcMotor，向下转型失败");
    return container_of(m, DcMotor, base);
}

#endif /* MODULE_DC_MOTOR_H */
