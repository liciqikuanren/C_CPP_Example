#ifndef TXN_PORT_H
#define TXN_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 临界区保护：单线程裸机主循环下为空实现；
 * 若在中断里调用 txn_on_rx()，可在这里加关中断/加锁 */
#ifndef TXN_LOCK
#define TXN_LOCK()   ((void)0)
#define TXN_UNLOCK() ((void)0)
#endif

/* 移植层：返回当前毫秒节拍，由用户实现（如 HAL_GetTick()） */
uint32_t txn_port_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif
