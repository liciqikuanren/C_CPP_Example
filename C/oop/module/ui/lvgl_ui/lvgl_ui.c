#include "module/ui/lvgl_ui/lvgl_ui.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include <assert.h>
#include <SDL2/SDL.h>

/* 控件事件回调：把用户操作经 Ui 的回调推给应用（和 console_ui 的 run 里做的事一样） */

static void on_switch_changed(lv_event_t* e) {
    LvglUi* self = (LvglUi*)lv_event_get_user_data(e);
    bool on = lv_obj_has_state(self->sw, LV_STATE_CHECKED);
    if (self->base.handler) {
        self->base.handler(self->base.user_data,
                           on ? UI_EV_SWITCH_ON : UI_EV_SWITCH_OFF, 0);
    }
}

static void on_slider_changed(lv_event_t* e) {
    LvglUi* self = (LvglUi*)lv_event_get_user_data(e);
    int v = (int)lv_slider_get_value(self->slider);
    if (self->base.handler) {
        self->base.handler(self->base.user_data, UI_EV_SPEED, v);
    }
}

/* ---- UiOps 实现 ---- */

static void lvgl_show_switch(Ui* self, bool on) {
    LvglUi* ui = (LvglUi*)self;   /* base 是首成员，地址相同 */
    if (on) lv_obj_add_state(ui->sw, LV_STATE_CHECKED);
    else    lv_obj_remove_state(ui->sw, LV_STATE_CHECKED);
}

/* 设定转速 → 滑条 */
static void lvgl_show_speed(Ui* self, int value) {
    LvglUi* ui = (LvglUi*)self;
    lv_slider_set_value(ui->slider, value, LV_ANIM_OFF);
}

/* 实际转速 → 数值标签（注意：默认字体没有中文，标签用英文/数字） */
static void lvgl_show_actual_speed(Ui* self, int value) {
    LvglUi* ui = (LvglUi*)self;
    lv_label_set_text_fmt(ui->actual_label, "Actual speed: %d", value);
}

/* UI 主循环：泵 LVGL。事件由上面的控件回调同步推给应用 */
static void lvgl_run(Ui* self) {
    (void)self;
    while (1) {
        lv_timer_handler();
        lv_tick_inc(5);
        SDL_Delay(5);
    }
}

static const UiOps lvgl_ui_ops = {
    .show_switch        = lvgl_show_switch,
    .show_speed         = lvgl_show_speed,
    .show_actual_speed  = lvgl_show_actual_speed,
    .run                = lvgl_run,
};

void lvgl_ui_init(LvglUi* self) {
    lv_display_t* disp;
    lv_obj_t* label;

    assert(self != NULL);

    lv_init();

    disp = lv_sdl_window_create(480, 480);
    lv_sdl_window_set_title(disp, "Motor GUI (LVGL)");
    lv_sdl_mouse_create();

    self->base.ops       = &lvgl_ui_ops;
    self->base.handler   = NULL;
    self->base.user_data = NULL;

    /* 标题 */
    label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Motor Control (LVGL)");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);

    /* 开关：电机启停 */
    self->sw = lv_switch_create(lv_screen_active());
    lv_obj_align(self->sw, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_event_cb(self->sw, on_switch_changed, LV_EVENT_VALUE_CHANGED, self);

    /* 滑条：设定转速 */
    label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Set speed");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -50);

    self->slider = lv_slider_create(lv_screen_active());
    lv_obj_set_width(self->slider, 300);
    lv_slider_set_range(self->slider, 0, 100);
    lv_obj_align(self->slider, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(self->slider, on_slider_changed, LV_EVENT_VALUE_CHANGED, self);

    /* 实际转速显示（新增） */
    self->actual_label = lv_label_create(lv_screen_active());
    lv_label_set_text(self->actual_label, "Actual speed: 0");
    lv_obj_align(self->actual_label, LV_ALIGN_CENTER, 0, 50);
}
