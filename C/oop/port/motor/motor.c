#include "port/motor/motor.h"
#include <assert.h>

/*
 * 统一的“ops 槽位未填写”检查：
 * 调用前先 assert 槽位非空，若该“虚函数”没填（NULL）则直接崩溃。
 */

void motor_init(Motor* self) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->init != NULL
           && "ops.init 未填写");
    self->ops->init(self);
}

void motor_set_speed(Motor* self, float speed) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->set_speed != NULL
           && "ops.set_speed 未填写");
    self->ops->set_speed(self, speed);
}

void motor_start(Motor* self) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->start != NULL
           && "ops.start 未填写");
    self->ops->start(self);
}

void motor_stop(Motor* self) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->stop != NULL
           && "ops.stop 未填写");
    self->ops->stop(self);
}

const char* motor_type_name(const Motor* self) {
    assert(self != NULL);
    assert(self->ops != NULL && self->ops->type_name != NULL
           && "ops.type_name 未填写");
    return self->ops->type_name(self);
}
/*字符串字面量永远不是 NULL，所以 && "..." 这一项永远为真，既不会让本该失败的情况“意外通过”，也不会让本该通过的情况“意外失败”。
一句话总结：&& "提示信息" 只是把一个「恒真的字符串」挂在表达式末尾，借用 assert 打印表达式原文的特性，把说明文字带出来而已。
*/

void motor_calibrate(Motor* self) {
    assert(self != NULL);
    /* calibrate 是可选槽位：没填（NULL）就崩溃 */
    assert(self->ops != NULL && self->ops->calibrate != NULL
           && "ops.calibrate 未填写");
    self->ops->calibrate(self);
}

void motor_brake(Motor* self) {
    assert(self != NULL);
    /* brake 是可选槽位：没填（NULL）就崩溃 */
    assert(self->ops != NULL && self->ops->brake != NULL
           && "ops.brake 未填写");
    self->ops->brake(self);
}

float motor_speed(const Motor* self) {
    assert(self != NULL);
    return self->speed;   /* 封装：只读访问 */
}

void motor_validate_ops(const Motor* self) {
    assert(self != NULL);
    assert(self->ops != NULL && "ops 表为空");
    /* 必填槽位必须全部填写，缺一个就崩溃 */
    assert(self->ops->init      != NULL && "必填槽位 init 未填写");
    assert(self->ops->set_speed != NULL && "必填槽位 set_speed 未填写");
    assert(self->ops->start     != NULL && "必填槽位 start 未填写");
    assert(self->ops->stop      != NULL && "必填槽位 stop 未填写");
    assert(self->ops->type_name != NULL && "必填槽位 type_name 未填写");
}

/* 查询可选槽位是否已填写（供 UI 判断某功能是否可用，避免误调用触发 assert） */
bool motor_supports_calibrate(const Motor* self) {
    return self != NULL && self->ops != NULL && self->ops->calibrate != NULL;
}

bool motor_supports_brake(const Motor* self) {
    return self != NULL && self->ops != NULL && self->ops->brake != NULL;
}
