#ifndef BSP_INPUT_H
#define BSP_INPUT_H

#include <stddef.h>
#include <stdbool.h>

/*
 * BSP 层：输入（按键）驱动。
 * 当前没有真实按键/触摸，用标准输入(键盘)模拟。
 */

/* 读取一行命令到 buf；成功返回 true，读到 EOF 返回 false。 */
bool bsp_input_get_line(char* buf, size_t len);

#endif /* BSP_INPUT_H */
