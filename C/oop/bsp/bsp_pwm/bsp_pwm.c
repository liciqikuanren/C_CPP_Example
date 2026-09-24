#include "bsp/bsp_pwm/bsp_pwm.h"
#include <stdio.h>

void bsp_pwm_set_duty(uint8_t channel, uint8_t percent) {
    printf("    [BSP-PWM] ch%u duty=%u%%\n",
           (unsigned)channel, (unsigned)percent);
}

void bsp_pwm_start(uint8_t channel) {
    printf("    [BSP-PWM] ch%u start\n", (unsigned)channel);
}

void bsp_pwm_stop(uint8_t channel) {
    printf("    [BSP-PWM] ch%u stop\n", (unsigned)channel);
}
