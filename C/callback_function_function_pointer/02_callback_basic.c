/*
 * 案例 2：回调函数的使用
 *
 * 回调函数 = 由别人调用的函数。
 * 通用逻辑（遍历）写在库函数里，具体“做什么”由调用者通过函数指针传入。
 */

#include <stdio.h>

#include "console_utf8.h"

/* 回调函数类型：接收一个元素，返回 void */
typedef void (*VisitCallback)(int value);

/* 通用遍历函数：不知道要做什么，只知道对每个元素调用一次 cb */
static void for_each(const int *array, int count, VisitCallback cb) {
    if (cb == NULL) {
        return;
    }
    for (int i = 0; i < count; i++) {
        cb(array[i]); /* 这里就是“回调”发生的地方 */
    }
}

/* ---------- 不同的回调实现：参数/遍历逻辑都不用改 ---------- */

static void print_item(int value) {
    printf("%d ", value);
}

static int g_sum = 0;
static void sum_item(int value) {
    g_sum += value;
}

static int g_even_count = 0;
static void count_even_item(int value) {
    if (value % 2 == 0) {
        g_even_count++;
    }
}

int main(void) {
    console_utf8_init();

    int data[] = { 1, 2, 3, 4, 5, 6 };
    int count = (int)(sizeof(data) / sizeof(data[0]));

    printf("遍历打印：");
    for_each(data, count, print_item);
    printf("\n");

    g_sum = 0;
    for_each(data, count, sum_item);
    printf("求和：%d\n", g_sum);

    g_even_count = 0;
    for_each(data, count, count_even_item);
    printf("偶数个数：%d\n", g_even_count);

    return 0;
}
