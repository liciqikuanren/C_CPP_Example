#include "bsp/bsp_input/bsp_input.h"
#include <stdio.h>
#include <string.h>

bool bsp_input_get_line(char* buf, size_t len) {
    size_t n;
    if (fgets(buf, (int)len, stdin) == NULL) {
        return false;   /* EOF 或读取失败 */
    }
    /* 去掉行尾换行符 */
    n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
    return true;
}
