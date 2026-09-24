#include "bsp/bsp_delay/bsp_delay.h"
#include <stdio.h>

void bsp_delay_ms(uint32_t ms) {
    printf("    [BSP-DELAY] %u ms\n", (unsigned)ms);
}
