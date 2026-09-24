#ifndef MODULE_MOTOR_H
#define MODULE_MOTOR_H

#include <stddef.h>
#include <stdbool.h>

/*
 * container_of：从「成员指针」反推「包含该成员的结构体指针」。
 * 原理：成员地址 - 该成员在结构体中的字节偏移(offsetof) = 结构体起始地址。
 * 用途：向下转型。不要求成员是结构体的第一个成员，放哪都行。
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/*
 * 模块层：Motor “基类”。
 *
 * 纯 C 里没有类、没有虚函数、没有 vtable，所以用两样东西手工模拟：
 *
 *   1) 结构体内嵌（继承）：派生结构体里内嵌一个 Motor 成员。
 *      向上转型：直接取成员地址 &obj->base。
 *      向下转型：用 container_of() 从成员指针反推结构体指针
 *      （不要求 base 是首成员，放哪都行）。
 *
 *   2) 函数指针表（ops 表 = vtable）：MotorOps 就是“虚函数表”。
 *      每个“虚函数”槽位是一个函数指针：
 *        - 必填槽位（init/set_speed/start/stop/type_name）：派生类型必须填写。
 *        - 可选槽位（calibrate/brake）：可不填（置 NULL）。
 *
 *  调用“虚函数”统一走 motor_xxx() 分发函数；分发函数会先 assert 槽位非空，
 *  若 ops 表里的函数没填，程序直接崩溃 —— 对应“虚函数没填就 assert 崩溃”。
 */

typedef struct Motor Motor;
typedef struct MotorOps MotorOps;

/* 类型标签：用于“向下转型”时的类型检查 */
typedef enum {
    MOTOR_DC = 0,
    MOTOR_STEPPER,
    MOTOR_KIND_COUNT
} MotorKind;

/* ops 表（vtable） */
struct MotorOps {
    void        (*init)(Motor* self);
    void        (*set_speed)(Motor* self, float speed);
    void        (*start)(Motor* self);
    void        (*stop)(Motor* self);
    const char* (*type_name)(const Motor* self);
    void        (*calibrate)(Motor* self);   /* 可选槽位：可不填 */
    void        (*brake)(Motor* self);       /* 可选槽位：可不填 */
};

/* 基类：内嵌在派生结构体中，实现“继承”（位置不限，向下转型用 container_of） */
struct Motor {
    const MotorOps* ops;   /* vtable 指针 */
    MotorKind       kind;  /* 类型标签，向下转型用 */
    float           speed; /* 封装字段：请用 motor_set_speed()/motor_speed() 访问 */
};

/* ---- “虚函数”分发接口（内部先 assert 检查 ops 槽位是否已填写）---- */
void        motor_init(Motor* self);
void        motor_set_speed(Motor* self, float speed);
void        motor_start(Motor* self);
void        motor_stop(Motor* self);
const char* motor_type_name(const Motor* self);
void        motor_calibrate(Motor* self);
void        motor_brake(Motor* self);

/* ---- 封装：只读访问 speed ---- */
float motor_speed(const Motor* self);

/* 查询可选槽位是否已填写（供 UI 判断某功能是否可用，避免误调用触发 assert） */
bool motor_supports_calibrate(const Motor* self);
bool motor_supports_brake(const Motor* self);

/* 构造时校验必填槽位已填写，否则 assert 崩溃 */
void motor_validate_ops(const Motor* self);

#endif /* MODULE_MOTOR_H */
