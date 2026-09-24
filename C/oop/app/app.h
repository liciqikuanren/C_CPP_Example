#ifndef APP_APP_H
#define APP_APP_H

/*
 * 应用层：装配电机模块并运行命令行控制台。
 * 本层只依赖模块层（module/motor）与 BSP 层，不含具体业务实现。
 * main.c 只作为程序入口，调用这里的接口。
 */

void app_init(void);    /* 初始化：BSP 初始化 + 创建并注册电机 */
void app_run(void);     /* 运行主循环：命令行控制台 */
void app_deinit(void);  /* 清理：销毁电机等资源 */

#endif /* APP_APP_H */
