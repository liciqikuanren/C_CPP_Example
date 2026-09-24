#include "port/ui/ui.h"
#include <assert.h>

void ui_show_switch(Ui* self, bool on) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->show_switch != NULL
           && "ops.show_switch 未填写");
    self->ops->show_switch(self, on);
}

void ui_show_speed(Ui* self, int value) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->show_speed != NULL
           && "ops.show_speed 未填写");
    self->ops->show_speed(self, value);
}

void ui_show_actual_speed(Ui* self, int value) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->show_actual_speed != NULL
           && "ops.show_actual_speed 未填写");
    self->ops->show_actual_speed(self, value);
}

void ui_run(Ui* self) {
    assert(self != NULL);
    assert(self->handler != NULL && "请先调用 ui_set_handler 注册事件回调");
    assert(self->ops != NULL && self->ops->run != NULL
           && "ops.run 未填写");
    self->ops->run(self);
}

void ui_set_handler(Ui* self, UiEventHandler handler, void* user_data) {
    assert(self != NULL);
    self->handler   = handler;
    self->user_data = user_data;
}
