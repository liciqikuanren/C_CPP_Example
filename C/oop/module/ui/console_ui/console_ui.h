#ifndef MODULE_UI_CONSOLE_UI_H
#define MODULE_UI_CONSOLE_UI_H

#include "port/ui/ui.h"

/*
 * 终端 UI：继承 Ui，用命令行“模拟”【开关 + 转速滑条】两个控件。
 * 屏幕未定型前用它测试；将来换 LVGL（lvgl_ui）实现同一张 ops 表即可。
 */
typedef struct ConsoleUi {
    Ui base;   /* 继承：首成员 */
} ConsoleUi;

void console_ui_init(ConsoleUi* self);

#endif /* MODULE_UI_CONSOLE_UI_H */
