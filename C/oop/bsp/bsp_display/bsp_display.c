#include "bsp/bsp_display/bsp_display.h"
#include <stdio.h>
#include <stdarg.h>

void bsp_display_init(void) {
    /* 模拟屏幕上电初始化（真实硬件时在这里初始化 LCD/OLED 控制器） */
}

void bsp_display_clear(void) {
    /* 清屏：打印 ANSI 转义；部分终端不支持也无害 */
    printf("\033[2J\033[H");
    fflush(stdout);
}

void bsp_display_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

void bsp_display_vprintf(const char* fmt, va_list ap) {
    vprintf(fmt, ap);
    fflush(stdout);
}
