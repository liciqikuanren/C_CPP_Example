#include "bsp/bsp_console/bsp_console.h"

#if defined(_WIN32)
#include <windows.h>
#endif

void bsp_console_init(void) {
#if defined(_WIN32)
    /* 中文 Windows 终端默认代码页是 GBK(936)，而本工程源文件/输出都是 UTF-8，
       直接 printf 中文会乱码；这里把控制台输入/输出代码页都设成 UTF-8(65001)。 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}
