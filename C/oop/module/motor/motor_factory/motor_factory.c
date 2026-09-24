#include "module/motor/motor_factory/motor_factory.h"
#include "module/motor/dc_motor/dc_motor.h"
#include "module/motor/stepper_motor/stepper_motor.h"
#include "bsp/bsp_gpio/bsp_gpio.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

Motor* motor_create(const char* type) {
    if (strcmp(type, "dc") == 0) {
        DcMotor* dc = (DcMotor*)malloc(sizeof(DcMotor));
        assert(dc != NULL && "malloc 失败");
        dc_motor_init(dc, 0, BSP_PIN_MOTOR_EN, BSP_PIN_MOTOR_DIR);
        return &dc->base;   /* 向上转型：取成员地址，与 base 位置无关 */
    }
    if (strcmp(type, "stepper") == 0) {
        StepperMotor* st = (StepperMotor*)malloc(sizeof(StepperMotor));
        assert(st != NULL && "malloc 失败");
        stepper_motor_init(st, BSP_PIN_STEP, BSP_PIN_MOTOR_DIR);
        return &st->base;   /* 向上转型 */
    }
    return NULL;
}

void motor_destroy(Motor* motor) {
    if (motor == NULL) return;
    /* 用 container_of 从基类成员指针反推派生结构体起始地址再 free。
       注意：StepperMotor 的 base 不是首成员，不能直接 free(motor)。 */
    switch (motor->kind) {
    case MOTOR_DC:
        free(container_of(motor, DcMotor, base));
        break;
    case MOTOR_STEPPER:
        free(container_of(motor, StepperMotor, base));
        break;
    default:
        assert(0 && "未知电机类型，无法释放");
        break;
    }
}
