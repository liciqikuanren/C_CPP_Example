#ifndef MODULE_UI_LVGL_UI_H
#define MODULE_UI_LVGL_UI_H

#include "lvgl.h"
#include "port/ui/ui.h"

/*
 * LVGL GUI：继承 Ui，用 lv_switch + lv_slider + lv_label 实现
 * “电机开关 + 转速滑条 + 实际转速显示”。
 * 这是最终目标实现；console_ui（命令行）是屏幕未定型前的临时替身。
 *
 * 事件通过 Ui 的 handler/user_data 回推给应用，逻辑与 console_ui 完全一致。
 */
typedef struct LvglUi {
    Ui        base;           /* 继承：首成员 */
    lv_obj_t* sw;             /* 开关控件：电机启停 */
    lv_obj_t* slider;         /* 滑条控件：设定转速 */
    lv_obj_t* actual_label;   /* 标签：显示实际转速 */
} LvglUi;

void lvgl_ui_init(LvglUi* self);

#endif /* MODULE_UI_LVGL_UI_H */
