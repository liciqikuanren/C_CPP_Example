#include "app/app.h"

/*
 * 程序入口：只负责调用应用层的初始化 / 运行 / 清理，不含任何业务逻辑。
 */
int main(void) {
    app_init();
    app_run();
    app_deinit();
    return 0;
}
