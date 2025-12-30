/* event_example.c
 * 加强反馈版：运行后会清晰显示每一步发生了什么
 * 编译：gcc event.c event_example.c -o event_test
 * 运行后会看到大量彩色输出（Windows cmd 支持部分颜色）
 */

#include "event.h"
#include <stdio.h>
#include <string.h>

// 定义一些事件类型
typedef enum {
    EVENT_BUTTON_PRESS = 1,   // 按钮按下
    EVENT_SENSOR_DATA,        // 传感器数据到达
    EVENT_SYSTEM_ALERT,       // 系统警报
    EVENT_USER_LOGIN          // 用户登录
} MyEventType;

// 优先级定义
#define PRIORITY_LOW     0
#define PRIORITY_NORMAL  1
#define PRIORITY_HIGH    2

// 回调1：处理按钮按下事件
void on_button_press(Event_t* event, void* arg)
{
    const char* button_name = (const char*)arg;
    printf("\033[1;33m[回调触发] 按钮事件处理中...\033[0m\n");
    printf("   → 按钮名称: %s\n", button_name);
    printf("   → 时间戳: %u ms\n", event->timestamp);
    printf("   → 优先级: %d\n\n", event->priority);
}

// 回调2：处理传感器数据
void on_sensor_data(Event_t* event, void* arg)
{
    (void)arg;  // 未使用
    printf("\033[1;32m[回调触发] 传感器数据已到达！\033[0m\n");
    if (event->data_size > 0) {
        printf("   → 数据长度: %d 字节\n", event->data_size);
        printf("   → 数据内容: ");
        for (uint8_t i = 0; i < event->data_size; i++) {
            printf("%02X ", event->data[i]);
        }
        printf("\n");
        // 假设是温度数据（示例）
        if (event->data_size >= 2) {
            int temp = (event->data[0] << 8) | event->data[1];
            printf("   → 解析温度: %.1f °C\n", temp / 10.0);
        }
    }
    printf("\n");
}

// 回调3：处理系统警报
void on_system_alert(Event_t* event, void* arg)
{
    int* alert_level = (int*)arg;
    printf("\033[1;31m[紧急回调] 系统警报触发！\033[0m\n");
    printf("   → 警报级别: %d\n", *alert_level);
    printf("   → 事件时间: %u ms\n\n", event->timestamp);
}

// 全局观察者：监控所有事件（最明显的反馈）
void global_observer(Event_t* event, void* arg)
{
    (void)arg;
    static const char* type_names[] = {
        "未知", "按钮按下", "传感器数据", "系统警报", "用户登录"
    };
    const char* type_name = (event->type < 5) ? type_names[event->type] : "其他事件";

    printf("\033[1;36m=== 全局观察者捕获事件 ===\033[0m\n");
    printf("   类型ID: %u → %s\n", event->type, type_name);
    printf("   优先级: %d\n", event->priority);
    printf("   时间戳: %u ms\n", event->timestamp);
    printf("   数据大小: %d 字节\n", event->data_size);
    printf("\033[1;36m==========================\033[0m\n\n");
}

int main(void)
{
    printf("\033[1;34m========== 事件系统完整演示开始 ==========\033[0m\n\n");

    EVENT_Init();  // 初始化

    // 订阅各种事件
    EVENT_Subscribe(EVENT_BUTTON_PRESS, on_button_press, "启动按钮");
    EVENT_Subscribe(EVENT_BUTTON_PRESS, on_button_press, "停止按钮");
    EVENT_Subscribe(EVENT_SENSOR_DATA, on_sensor_data, NULL);

    int alert_level = 3;
    EVENT_Subscribe(EVENT_SYSTEM_ALERT, on_system_alert, &alert_level);

    // 注册全局观察者（会看到所有事件）
    EVENT_RegisterObserver(global_observer, NULL);

    printf("\033[1;35m→ 订阅和观察者注册完成，开始发布事件...\033[0m\n\n");

    // 发布各种事件
    EVENT_Publish(EVENT_BUTTON_PRESS, PRIORITY_HIGH, NULL, 0);

    uint8_t sensor_data[] = {0x01, 0x68};  // 示例：36.8°C → 0x0168 (368)
    EVENT_Publish(EVENT_SENSOR_DATA, PRIORITY_NORMAL, sensor_data, sizeof(sensor_data));

    const char* login_user = "admin";
    EVENT_Publish(EVENT_USER_LOGIN, PRIORITY_NORMAL, login_user, strlen(login_user) + 1);

    EVENT_Publish(EVENT_SYSTEM_ALERT, PRIORITY_HIGH, "电源故障", 10);

    // 处理所有事件（关键一步！）
    printf("\033[1;35m→ 开始处理队列中的事件...\033[0m\n\n");
    int processed = EVENT_Process();

    printf("\033[1;32m本次共处理了 %d 个事件\033[0m\n", processed);
    printf("当前队列剩余事件: %u 个\n", EVENT_GetCount());

    printf("\n\033[1;34m========== 演示结束 ==========\033[0m\n");

    // 暂停让窗口不闪退（Dev-C++ 必备）
    printf("\n按回车键退出程序...");
    getchar();

    return 0;
}
