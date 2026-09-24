#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include <stdarg.h>

/*
 * BSP 层：屏幕（显示）驱动。
 * 当前没有真实屏幕，用终端(printf)模拟。
 * 接入真实 LCD/OLED 时，只需替换 bsp_display.c 的实现，上层零改动。
 */

void bsp_display_init(void);                       /* 初始化屏幕 */
void bsp_display_clear(void);                      /* 清屏 */
void bsp_display_printf(const char* fmt, ...);     /* 往“屏幕”上输出一段 */
void bsp_display_vprintf(const char* fmt, va_list ap); /* vprintf 变体，供 ops 表转发 */

#endif /* BSP_DISPLAY_H */
