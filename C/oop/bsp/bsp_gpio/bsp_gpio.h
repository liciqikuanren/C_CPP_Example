#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/*
 * BSP 层：GPIO "驱动"。
 * 当前不接任何硬件，用 printf 模拟引脚电平变化。
 * 后续接入真实芯片时，只需替换 bsp_gpio.c 的实现，
 * 上层（module / app）代码无需任何改动 —— 这就是分层的意义。
 */

typedef uint8_t bsp_pin_t;

/* 工程中用到的虚拟引脚编号 */
#define BSP_PIN_MOTOR_EN  ((bsp_pin_t)0)  /* 电机使能 */
#define BSP_PIN_MOTOR_DIR ((bsp_pin_t)1)  /* 电机方向 */
#define BSP_PIN_STEP      ((bsp_pin_t)2)  /* 步进电机脉冲 */

void bsp_gpio_write(bsp_pin_t pin, bool level);
bool bsp_gpio_read(bsp_pin_t pin);

#endif /* BSP_GPIO_H */
