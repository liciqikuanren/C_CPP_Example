#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>

/*
 * BSP 层：PWM "驱动"。当前用 printf 模拟占空比输出。
 */
void bsp_pwm_set_duty(uint8_t channel, uint8_t percent); /* 0~100 */
void bsp_pwm_start(uint8_t channel);
void bsp_pwm_stop(uint8_t channel);

#endif /* BSP_PWM_H */
