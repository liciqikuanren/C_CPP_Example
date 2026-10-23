/*
 * 案例 1：函数指针的定义方法与使用方法
 *
 * 函数指针 = 指向函数的指针，通过它可以间接调用函数。
 * 定义的关键：把函数名换成 (*指针变量名)，其余类型与目标函数保持一致。
 */

#include <stdio.h>

#include "console_utf8.h"

/* 被指向的普通函数 */
static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }
static int divide_op(int a, int b) { return (b != 0) ? (a / b) : 0; }

/* ---------- 函数指针的三种定义方法 ---------- */

/* 方法 1：直接定义变量（类型写在变量名周围）
 * int (*fp)(int, int);
 */

/* 方法 2：先用 typedef 定义“函数指针类型”，再用它定义变量（工程中最常用） */
typedef int (*BinaryOp)(int, int);

/* 方法 3：先 typedef “函数类型”，再用该类型定义指针 */
typedef int BinaryOpFunc(int, int);
typedef BinaryOpFunc *BinaryOpPtr;

int main(void) {
    console_utf8_init();

    /* 方法 1：定义 + 初始化 */
    int (*fp)(int, int) = add;
    printf("方法1 fp(3, 4)        = %d\n", fp(3, 4));

    /* 方法 2：注意 BinaryOp 是“指针类型”，op 本身就是指针 */
    BinaryOp op = mul;
    printf("方法2 op(3, 4)        = %d\n", op(3, 4));

    /* 方法 3：先用函数类型定义指针，再初始化 */
    BinaryOpPtr op2 = sub;
    printf("方法3 op2(10, 4)      = %d\n", op2(10, 4));

    /* 两种调用写法完全等价 */
    printf("两种写法 op(6,7)=%d, (*op)(6,7)=%d\n", op(6, 7), (*op)(6, 7));

    /* 函数指针可以重新指向别的函数 */
    op = add;
    printf("改指 add 后 op(6, 7)  = %d\n", op(6, 7));

    /* 函数指针数组：一个循环调用不同函数 */
    BinaryOp table[] = { add, sub, mul, divide_op };
    const char *names[] = { "+", "-", "*", "/" };
    int count = (int)(sizeof(table) / sizeof(table[0]));

    for (int i = 0; i < count; i++) {
        printf("12 %s 4 = %d\n", names[i], table[i](12, 4));
    }

    /* 函数指针使用前必须判断是否为 NULL */
    BinaryOp empty = NULL;
    if (empty == NULL) {
        printf("空函数指针不能调用\n");
    }

    return 0;
}
