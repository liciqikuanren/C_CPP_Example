/*
 * 案例 4：标准库中的回调 —— qsort 比较函数
 *
 * 演示带返回值的回调：排序算法固定，比较规则由用户通过函数指针提供。
 */

#include <stdio.h>
#include <stdlib.h>

#include "console_utf8.h"

/* 比较回调类型：a < b 返回负数，相等返回 0，a > b 返回正数 */
typedef int (*CompareFunc)(const void *a, const void *b);

static int compare_int_asc(const void *a, const void *b) {
    int lhs = *(const int *)a;
    int rhs = *(const int *)b;
    return (lhs > rhs) - (lhs < rhs);
}

static int compare_int_desc(const void *a, const void *b) {
    return compare_int_asc(b, a); /* 参数反过来就是降序 */
}

/* 自己封装一层：把比较函数指针作为参数继续传递 */
static void sort_ints(int *array, int count, CompareFunc cmp) {
    qsort(array, (size_t)count, sizeof(array[0]), cmp);
}

static void print_array(const char *tag, const int *array, int count) {
    printf("%s", tag);
    for (int i = 0; i < count; i++) {
        printf("%d ", array[i]);
    }
    printf("\n");
}

int main(void) {
    console_utf8_init();

    int data[] = { 42, 7, 19, 3, 88, 56, 1 };
    int count = (int)(sizeof(data) / sizeof(data[0]));

    sort_ints(data, count, compare_int_asc);
    print_array("升序：", data, count);

    sort_ints(data, count, compare_int_desc);
    print_array("降序：", data, count);

    return 0;
}
