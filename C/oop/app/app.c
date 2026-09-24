#include "app/app.h"
#include "port/motor/motor.h"
#include "module/motor/motor_factory/motor_factory.h"
#include "app/motor_ui/motor_ui.h"
#include "bsp/bsp_console/bsp_console.h"
#include <assert.h>

/*
 * 组合根：唯一允许同时认识 port 和 module 的地方。
 *
 * 用宏 USE_LVGL_UI 二选一：
 *   - 定义   → LVGL GUI（开关 + 滑条）—— 最终目标
 *   - 不定义 → 命令行控制台（on/off/speed）—— 屏幕没定型前的临时替身
 * 两者实现的是同一张 UiOps 接口，motor_ui（控制面板逻辑）完全无感。
 *
 * ⚠⚠ 不要在这里写 #define USE_LVGL_UI ！！！
 *     USE_LVGL_UI 必须由【构建系统】传入：
 *       - CMake GUI 版：   cmake ... -DBUILD_LVGL_GUI=ON
 *       - 手动 gcc GUI 版： gcc ... -DUSE_LVGL_UI
 *     若写死在这里，CMake 的“命令行版”也会去找 lvgl.h，
 *     而 CMake 工程默认没接 LVGL → fatal error C1083: 无法打开 "lvgl.h"。
 */
#ifdef USE_LVGL_UI
#include "module/ui/lvgl_ui/lvgl_ui.h"
static LvglUi s_ui;
#else
#include "module/ui/console_ui/console_ui.h"
#include "bsp/bsp_display/bsp_display.h"
static ConsoleUi s_ui;
#endif

/* 本工程依赖 assert 演示“ops 表未填写 -> 崩溃”，
   必须用 Debug 构建（不要定义 NDEBUG）。 */
#if defined(NDEBUG)
#error "请以 Debug 模式编译（不要定义 NDEBUG），否则 assert 不生效"
#endif

static Motor* s_motor = NULL;

void app_init(void) {
    bsp_console_init();   /* 控制台 UTF-8（两种 UI 下都留着，方便打日志） */

#ifdef USE_LVGL_UI
    lvgl_ui_init(&s_ui);    /* LVGL GUI：真开关 + 真滑条 */
#else
    bsp_display_init();     /* 终端“屏幕” */
    console_ui_init(&s_ui); /* 命令行：文字模拟开关 + 滑条 */
#endif

    /* 工厂创建电机 + 向上转型（DcMotor* -> Motor*） */
    s_motor = motor_create("dc");
    assert(s_motor != NULL);
}

void app_run(void) {
    /* 向上转型：LvglUi* 或 ConsoleUi* -> Ui*，交给控制面板逻辑（多态） */
    motor_ui_run(&s_ui.base, s_motor);
}

void app_deinit(void) {
    motor_destroy(s_motor);
    s_motor = NULL;
}
