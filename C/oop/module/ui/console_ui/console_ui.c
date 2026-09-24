#include "module/ui/console_ui/console_ui.h"
#include "bsp/bsp_display/bsp_display.h"
#include "bsp/bsp_input/bsp_input.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void console_show_switch(Ui* self, bool on) {
    (void)self;
    bsp_display_printf("  [开关] %s\n", on ? "ON" : "OFF");
}

/* 回显【设定】转速（对应 GUI 里的滑条） */
static void console_show_speed(Ui* self, int value) {
    (void)self;
    bsp_display_printf("  [设定转速] %d\n", value);
}

/* 回显【实际】转速（对应 GUI 里的数值显示） */
static void console_show_actual_speed(Ui* self, int value) {
    (void)self;
    bsp_display_printf("  [实际转速] %d\n", value);
}

/* UI 主循环：读命令行，把文字操作翻译成事件，通过回调推给应用 */
static void console_run(Ui* self) {
    char line[64];

    while (1) {
        bsp_display_printf("> ");
        if (!bsp_input_get_line(line, sizeof(line))) {
            self->handler(self->user_data, UI_EV_QUIT, 0);   /* EOF 视为退出 */
            return;
        }

        if (strcmp(line, "on") == 0) {
            self->handler(self->user_data, UI_EV_SWITCH_ON, 0);
        } else if (strcmp(line, "off") == 0) {
            self->handler(self->user_data, UI_EV_SWITCH_OFF, 0);
        } else if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            self->handler(self->user_data, UI_EV_QUIT, 0);
            return;
        } else if (strncmp(line, "speed", 5) == 0) {
            self->handler(self->user_data, UI_EV_SPEED, atoi(line + 5));
        } else {
            bsp_display_printf("  未知命令: %s（可用: on / off / speed <值> / quit）\n", line);
        }
    }
}

/* ConsoleUi 的 ops 表（vtable） */
static const UiOps console_ui_ops = {
    .show_switch        = console_show_switch,
    .show_speed         = console_show_speed,
    .show_actual_speed  = console_show_actual_speed,
    .run                = console_run,
};

void console_ui_init(ConsoleUi* self) {
    assert(self != NULL);
    self->base.ops       = &console_ui_ops;
    self->base.handler   = NULL;
    self->base.user_data = NULL;
}
