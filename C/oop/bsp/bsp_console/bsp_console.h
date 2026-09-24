#ifndef BSP_CONSOLE_H
#define BSP_CONSOLE_H

/*
 * BSP 层：控制台初始化。
 * Windows 下把控制台代码页设为 UTF-8，避免 printf 中文乱码；
 * 其他平台默认就是 UTF-8，无需处理。
 */
void bsp_console_init(void);

#endif /* BSP_CONSOLE_H */
