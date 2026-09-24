# 电机控制 OOP 案例（纯 C，分层：应用层 / 接口层 / 模块层 / BSP 层）

用“电机控制系统”在**纯 C** 里手工实现面向对象的核心思想。
C 没有类/虚函数/vtable，所以用「结构体 + 函数指针表（ops 表）」来模拟。
最终 GUI 是两个控件——【电机开关】+【转速滑条】。屏幕尚未定型，先用命令行模拟这两个控件，
输入 `on/off/speed <值>` 来测试业务逻辑。

## OOP 概念 → C 实现对照

| 面向对象概念 | 纯 C 实现方式 | 本案例位置 |
| --- | --- | --- |
| 封装 (Encapsulation) | 字段约定私有（`speed`/`running`），只经接口函数访问 | `motor_set_speed()/motor_speed()` |
| 继承 (Inheritance) | 结构体内嵌：派生结构体内嵌 `Motor` 成员（位置不限） | `DcMotor.base` / `StepperMotor.base` |
| 虚函数 / vtable | **函数指针表 `MotorOps`（ops 表）** | `port/motor/motor.h` |
| 多态 (Polymorphism) | 通过 `motor->ops->xxx()` 分发，行为随具体类型而变 | `motor_xxx()` 分发函数 |
| 向上转型 (Upcasting) | 取成员地址 `&obj->base`（不依赖成员位置） | `return &dc->base;` |
| 向下转型 (Downcasting) | `container_of()` 反推结构体 + 类型标签 `kind` 校验 | `motor_as_dc()/motor_as_stepper()` |
| 开闭原则 (OCP) | 新增类型 = 新结构体 + 工厂加分支，旧代码不动 | `motor_create()` |
| ops 未填写则崩溃 | 调用前 `assert(槽位 != NULL)` | `motor.c` 分发函数 |

> 说明：向下转型用 `container_of`（见 `port/motor/motor.h`）。`DcMotor` 把 `base` 放在首位（`offsetof==0`），
> `StepperMotor` 故意把 `base` 放在非首位，用来证明 `container_of` 不依赖成员顺序；
> 若仍用旧的“首成员强转”写法，`StepperMotor` 就会得到错误地址。

## 目录结构（四层）

```
oop/
├── app/                 # 应用层（业务逻辑只依赖 port）
│   ├── main.c                 程序入口（仅调用 app_init/app_run/app_deinit）
│   ├── app.h/.c               装配（组合根，允许认识 module 来创建对象）
│   └── motor_ui/              控制面板逻辑（开关+滑条，只依赖 port）
├── port/                # 接口层（接口 + ops 表 + 分发函数，零依赖）
│   ├── motor/                 Motor 接口（Motor + MotorOps + 分发 + container_of）
│   └── ui/                    Ui 接口（Ui + UiOps + 分发）
├── module/              # 模块层（实现，依赖 port + bsp）
│   ├── motor/
│   │   ├── dc_motor/           直流电机（实现 Motor 接口）
│   │   ├── stepper_motor/      步进电机（实现 Motor 接口）
│   │   └── motor_factory/      工厂（认识所有具体电机）
│   └── ui/
│       └── console_ui/         终端 UI（实现 Ui 接口）
├── bsp/                 # BSP 层：硬件桩（每个模块一个文件夹）
│   ├── bsp_gpio/               GPIO 模拟
│   ├── bsp_pwm/                PWM 模拟
│   ├── bsp_delay/              延时模拟
│   ├── bsp_console/            控制台 UTF-8 初始化
│   ├── bsp_display/            “屏幕”模拟
│   └── bsp_input/              “按键”模拟
├── CMakeLists.txt
└── README.md
```

依赖方向：`app -> port <- module -> bsp`。
- app 只依赖 port（接口）；module 依赖 port（实现接口）和 bsp（硬件）。
- 具体实现向上依赖抽象（依赖倒置），抽象不依赖具体实现。
- 只有 app 的“装配点”（app.c）允许认识 module 去创建对象。

## 关键设计：ops 表与“未填写就崩溃”

`MotorOps` 就是 ops 表（等价于 C++ 的 vtable），每个函数指针是一个“虚函数”槽位：

1. **必填槽位**（`init/set_speed/start/stop/type_name`）：派生类型必须填写。
   构造时用 `motor_validate_ops()` 检查，缺失直接 `assert` 崩溃。

2. **可选槽位**（`calibrate/brake`）：可不填（置 `NULL`）。
   调用统一走 `motor_calibrate()/motor_brake()`，分发前 `assert(槽位 != NULL)`，
   没填就直接崩溃。

本案例刻意安排：
- `DcMotor` 填了 `brake`，没填 `calibrate`
- `StepperMotor` 填了 `calibrate`，没填 `brake`

## 构建与运行

前置：`gcc`（MinGW）或 `cmake + ninja`。

### 方式一：直接 gcc

```powershell
gcc -std=c11 -I. app/main.c app/app.c app/motor_ui/motor_ui.c `
    port/motor/motor.c port/ui/ui.c `
    module/motor/dc_motor/dc_motor.c module/motor/stepper_motor/stepper_motor.c `
    module/motor/motor_factory/motor_factory.c module/ui/console_ui/console_ui.c `
    bsp/bsp_gpio/bsp_gpio.c bsp/bsp_pwm/bsp_pwm.c bsp/bsp_delay/bsp_delay.c `
    bsp/bsp_console/bsp_console.c bsp/bsp_display/bsp_display.c bsp/bsp_input/bsp_input.c `
    -o build/motor_oop_demo.exe
.\build\motor_oop_demo.exe
```

### 方式二：CMake

```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\motor_oop_demo.exe
```

## 命令行用法（模拟“开关 + 转速滑条”）

程序启动后进入交互循环，用文字模拟两个控件：

| 输入 | 模拟的控件操作 | 效果 |
| --- | --- | --- |
| `on` | 开关拨到 ON | 电机启动 |
| `off` | 开关拨到 OFF | 电机停止 |
| `speed 66` | 滑条滑到 66 | 设置转速 |
| `quit` | 退出 | 停止电机并退出 |

将来换 LVGL 时，这两个“文字操作”会变成真正的【开关控件】和【滑条控件】，
但 `UiOps` 接口（`show_switch` / `show_speed` / `run`，配合 `ui_set_handler` 回调）和电机控制逻辑都不变。

> 注意：必须以 Debug 模式编译（**不要定义 NDEBUG**），否则 `assert` 会被剥离。
> 代码里已加 `#error` 保护，误用 Release 编译会直接报错提醒。

## 关于中文乱码

程序输出是 UTF-8，而中文 Windows 终端默认代码页是 GBK(936)，直接运行会乱码。
`bsp_console_init()`（`bsp/bsp_console.c`）会在程序启动时把控制台代码页设为 UTF-8，
在 VS Code 终端 / cmd / PowerShell 里运行都不会乱码。

若仍有乱码，可在运行前手动执行 `chcp 65001` 再运行。
