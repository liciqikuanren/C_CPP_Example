/*
 * LVGL SDL 模拟器：最小入口。
 * 只依赖 LVGL + SDL2，不涉及电机项目，用来验证“屏幕没选好时先在 PC 上跑 GUI”。
 *
 * 要点：
 *  - Windows 上 SDL.h 会把 main 重定义成 SDL_main，所以 main 要写 int main(int, char**)。
 *  - lv_sdl_window_create() 只建窗口+显示，【不会】自动注册鼠标/键盘输入设备，
 *    必须再调 lv_sdl_mouse_create() / lv_sdl_keyboard_create()，否则点不动控件。
 */
#include "lvgl.h"
#include "lvgl/drivers/sdl/lv_sdl_window.h"
#include "lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/drivers/sdl/lv_sdl_keyboard.h"
#include <SDL2/SDL.h>

static int click_count = 0;

static void on_btn_clicked(lv_event_t* e) {
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
    click_count++;
    lv_label_set_text_fmt(label, "Clicked: %d", click_count);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    lv_init();

    lv_display_t* disp = lv_sdl_window_create(480, 320);
    lv_sdl_window_set_title(disp, "LVGL SDL Simulator");

    /* 关键：注册鼠标/键盘输入设备，否则窗口收不到点击 */
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    lv_obj_t* title = lv_label_create(lv_screen_active());
    lv_label_set_text(title, "Hello, LVGL!");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t* btn = lv_button_create(lv_screen_active());
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click me");
    lv_obj_center(btn_label);

    lv_obj_t* hint = lv_label_create(lv_screen_active());
    lv_label_set_text(hint, "Clicked: 0");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_add_event_cb(btn, on_btn_clicked, LV_EVENT_CLICKED, hint);

    while (1) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}
