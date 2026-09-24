#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include <stdint.h>

/* 毫秒延时。当前不接硬件，只打印，不真正阻塞。 */
void bsp_delay_ms(uint32_t ms);

#endif /* BSP_DELAY_H */
