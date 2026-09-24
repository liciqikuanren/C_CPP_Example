#include "module/motor/stepper_motor/stepper_motor.h"
#include "bsp/bsp_delay/bsp_delay.h"
#include <assert.h>
#include <stdio.h>

static void stepper_init(Motor* self) {
    StepperMotor* st = container_of(self, StepperMotor, base);   /* 向下转型 */
    printf("  [StepperMotor] init: step=pin%u dir=pin%u\n",
           (unsigned)st->step_pin, (unsigned)st->dir_pin);
    bsp_gpio_write(st->step_pin, false);
    bsp_gpio_write(st->dir_pin, false);
}

static void stepper_set_speed(Motor* self, float speed) {
    /* 步进电机的“速度”是脉冲频率，单位 步/s，范围与直流电机不同 */
    if (speed < 0.0f)    speed = 0.0f;
    if (speed > 1000.0f) speed = 1000.0f;
    self->speed = speed;
    printf("  [StepperMotor] setSpeed %.1f step/s\n", self->speed);
}

static void stepper_start(Motor* self) {
    StepperMotor* st = container_of(self, StepperMotor, base);
    int i;
    printf("  [StepperMotor] start: 开始产生脉冲\n");
    for (i = 0; i < 3; ++i) {   /* 模拟走 3 步 */
        bsp_gpio_write(st->step_pin, true);
        bsp_gpio_write(st->step_pin, false);
        bsp_delay_ms(1);        /* 用 BSP 延时模拟脉冲周期 */
    }
}

static void stepper_stop(Motor* self) {
    (void)self;
    printf("  [StepperMotor] stop\n");
}

static const char* stepper_type_name(const Motor* self) {
    (void)self;
    return "StepperMotor";
}

static void stepper_calibrate(Motor* self) {
    StepperMotor* st = container_of(self, StepperMotor, base);
    /* 填写了 ops 表里的“校准”槽位 */
    printf("  [StepperMotor] calibrate: 回原点\n");
    bsp_gpio_write(st->dir_pin, true);
}

/* StepperMotor 的 ops 表（vtable）：brake 故意留空(NULL) */
static const MotorOps stepper_motor_ops = {
    .init       = stepper_init,
    .set_speed  = stepper_set_speed,
    .start      = stepper_start,
    .stop       = stepper_stop,
    .type_name  = stepper_type_name,
    .calibrate  = stepper_calibrate, /* 已填写 */
    .brake      = NULL,              /* 未填写：调用会 assert 崩溃 */
};

void stepper_motor_init(StepperMotor* self, bsp_pin_t step_pin,
                        bsp_pin_t dir_pin) {
    assert(self != NULL);
    self->base.ops   = &stepper_motor_ops;
    self->base.kind  = MOTOR_STEPPER;
    self->base.speed = 0.0f;
    self->step_pin   = step_pin;
    self->dir_pin    = dir_pin;
    motor_validate_ops(&self->base);
}
