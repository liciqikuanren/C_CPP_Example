/*
 * 让 Windows 控制台按 UTF-8 显示输出。
 *
 * 源码与执行字符集都是 UTF-8（CMake 中使用了 /utf-8），
 * 而中文 Windows 控制台默认是 GBK(936)，所以需要切换一下代码页，
 * 否则 printf 出来的中文会变成乱码。
 */

#ifndef CONSOLE_UTF8_H
#define CONSOLE_UTF8_H

#ifdef _WIN32
#include <windows.h>

static void console_utf8_init(void) {
    SetConsoleOutputCP(CP_UTF8);
}
#else
static void console_utf8_init(void) {
}
#endif

#endif
