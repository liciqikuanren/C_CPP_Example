#include "module/motor/dc_motor/dc_motor.h"
#include "bsp/bsp_pwm/bsp_pwm.h"
#include <assert.h>
#include <stdio.h>

static void dc_init(Motor* self) {
    DcMotor* dc = container_of(self, DcMotor, base);   /* 向下转型 */
    printf("  [DcMotor] init: en=pin%u dir=pin%u pwm=ch%u\n",
           (unsigned)dc->en_pin, (unsigned)dc->dir_pin,
           (unsigned)dc->pwm_channel);
    bsp_gpio_write(dc->en_pin, false);
    bsp_gpio_write(dc->dir_pin, false);
    bsp_pwm_set_duty(dc->pwm_channel, 0);
}

static void dc_set_speed(Motor* self, float speed) {
    DcMotor* dc = container_of(self, DcMotor, base);
    /* 封装：在接口内做范围校验，外部无法写入非法速度 */
    if (speed < 0.0f)   speed = 0.0f;
    if (speed > 100.0f) speed = 100.0f;
    self->speed = speed;

    uint8_t duty = (uint8_t)speed;
    bsp_pwm_set_duty(dc->pwm_channel, duty);
    printf("  [DcMotor] setSpeed %.1f -> duty %u%%\n",
           self->speed, (unsigned)duty);
}

static void dc_start(Motor* self) {
    DcMotor* dc = container_of(self, DcMotor, base);
    bsp_gpio_write(dc->en_pin, true);
    bsp_pwm_start(dc->pwm_channel);
    dc->running = true;
    printf("  [DcMotor] start: 使能 + 启动 PWM\n");
}

static void dc_stop(Motor* self) {
    DcMotor* dc = container_of(self, DcMotor, base);
    bsp_pwm_stop(dc->pwm_channel);
    bsp_gpio_write(dc->en_pin, false);
    dc->running = false;
    printf("  [DcMotor] stop\n");
}

static const char* dc_type_name(const Motor* self) {
    (void)self;
    return "DcMotor";
}

static void dc_brake(Motor* self) {
    DcMotor* dc = container_of(self, DcMotor, base);
    /* 填写了 ops 表里的“刹车”槽位 */
    bsp_pwm_set_duty(dc->pwm_channel, 0);
    printf("  [DcMotor] brake: 短路制动\n");
}

/* DcMotor 的 ops 表（vtable）：calibrate 故意留空(NULL) */
static const MotorOps dc_motor_ops = {
    .init       = dc_init,
    .set_speed  = dc_set_speed,
    .start      = dc_start,
    .stop       = dc_stop,
    .type_name  = dc_type_name,
    .calibrate  = NULL,      /* 未填写：调用会 assert 崩溃 */
    .brake      = dc_brake,  /* 已填写 */
};

void dc_motor_init(DcMotor* self, uint8_t pwm_channel,
                   bsp_pin_t en_pin, bsp_pin_t dir_pin) {
    assert(self != NULL);
    self->base.ops    = &dc_motor_ops;
    self->base.kind   = MOTOR_DC;
    self->base.speed  = 0.0f;
    self->pwm_channel = pwm_channel;
    self->en_pin      = en_pin;
    self->dir_pin     = dir_pin;
    self->running     = false;
    motor_validate_ops(&self->base);   /* 校验必填槽位已填写 */
}
