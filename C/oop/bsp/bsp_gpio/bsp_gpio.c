#include "bsp/bsp_gpio/bsp_gpio.h"
#include <stdio.h>

void bsp_gpio_write(bsp_pin_t pin, bool level) {
    /* TODO(硬件): 接入真实芯片后改为写 GPIO 寄存器。 */
    printf("    [BSP-GPIO] pin %u -> %s\n",
           (unsigned)pin, level ? "HIGH" : "LOW");
}

bool bsp_gpio_read(bsp_pin_t pin) {
    (void)pin;
    /* 模拟读回低电平。 */
    return false;
}
