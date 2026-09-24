cmake_minimum_required(VERSION 3.16)
project(motor_oop_demo C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 关键：本案例用 assert 演示“ops 表未填写时崩溃”。
# Release 编译会定义 NDEBUG、剥离 assert，导致演示失效，故默认强制 Debug。
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ============================================================
# 两个构建目标，用 BUILD_LVGL_GUI 二选一：
#
#   （默认）命令行版：
#     cmake -S . -B build -G Ninja
#
#   LVGL GUI 版（开关 + 滑条）：
#     cmake -S . -B build -DBUILD_LVGL_GUI=ON
#     （SDL2_DIR 会自动定位 third_party/SDL2-*/cmake 或
#       third_party/SDL2-*/x86_64-w64-mingw32/lib/cmake/SDL2；
#       找不到时用 -DSDL2_DIR=<SDL2 的 cmake 目录> 手动指定）
#
#   ⚠ SDL2 必须跟工具链匹配：
#      - MSVC      → 用 SDL2 的 VC 包（SDL2-devel-*-VC.zip）
#      - MinGW gcc → 用 SDL2 的 mingw 包（SDL2-devel-*-mingw.tar.gz）
#        （两边库格式不兼容，不能混用）
# ============================================================
option(BUILD_LVGL_GUI "Build LVGL GUI (switch + slider) instead of console UI" ON)

# ---- 公共源文件 ----
set(APP_SOURCES
    app/main.c
    app/app.c
    app/motor_ui/motor_ui.c
    port/motor/motor.c
    port/ui/ui.c
    module/motor/dc_motor/dc_motor.c
    module/motor/stepper_motor/stepper_motor.c
    module/motor/motor_factory/motor_factory.c
    bsp/bsp_gpio/bsp_gpio.c
    bsp/bsp_pwm/bsp_pwm.c
    bsp/bsp_delay/bsp_delay.c
    bsp/bsp_console/bsp_console.c
)

if(BUILD_LVGL_GUI)
    list(APPEND APP_SOURCES
        module/ui/lvgl_ui/lvgl_ui.c
    )
else()
    list(APPEND APP_SOURCES
        module/ui/console_ui/console_ui.c
        bsp/bsp_display/bsp_display.c
        bsp/bsp_input/bsp_input.c
    )
endif()

add_executable(motor_oop_demo ${APP_SOURCES})

# 让源码里可以直接 #include "module/xxx.h" / "bsp/xxx.h"
target_include_directories(motor_oop_demo PRIVATE ${CMAKE_SOURCE_DIR})

# MSVC 默认按本地代码页(中文 Windows 为 936/GBK)读取源码，而本工程源文件是 UTF-8，
# 会导致 C4819 告警并可能让中文输出乱码；加 /utf-8 让 MSVC 按 UTF-8 读取。
if(MSVC)
    target_compile_options(motor_oop_demo PRIVATE /utf-8)
endif()

# ---- LVGL GUI 版附加配置 ----
if(BUILD_LVGL_GUI)
    # 让 app.c 走 LVGL 分支（等价于命令行 -DUSE_LVGL_UI）
    target_compile_definitions(motor_oop_demo PRIVATE USE_LVGL_UI)

    # LVGL 的配置文件（third_party/lv_conf.h 里已开 LV_USE_SDL）
    set(LV_BUILD_CONF_PATH "${CMAKE_SOURCE_DIR}/third_party/lv_conf.h"
        CACHE PATH "Path to lv_conf.h used by LVGL")

    # 用本地已装好的 SDL2，不要联网 FetchContent
    set(LV_FETCH_SDL2 OFF CACHE BOOL "Fetch SDL2 from source" FORCE)
    set(LV_USE_FIND_PACKAGE_SDL2 ON CACHE BOOL "Resolve SDL2 via find_package" FORCE)

    # 关掉 LVGL 自带的 examples / demos：我们不需要，而且它们含非 ASCII 字符，
    # MSVC 按本地代码页 GBK(936) 读会报 “error C2001: 常量中有换行符” 而构建失败
    set(CONFIG_LV_BUILD_DEMOS    OFF CACHE BOOL "Build LVGL demos" FORCE)
    set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)

    # 自动定位本地解压的 SDL2（MinGW x64 版）；可用 -DSDL2_DIR=... 覆盖
    if(NOT SDL2_DIR)
        file(GLOB _sdl2_cmake
             "${CMAKE_SOURCE_DIR}/third_party/SDL2-*/cmake"
             "${CMAKE_SOURCE_DIR}/third_party/SDL2-*/x86_64-w64-mingw32/lib/cmake/SDL2")
        if(_sdl2_cmake)
            list(GET _sdl2_cmake 0 SDL2_DIR)
        endif()
    endif()
    message(STATUS "GUI: SDL2_DIR = ${SDL2_DIR}")

    # 关键：先自己找到 SDL2。否则 LVGL 的 SDL 驱动编不过
    # （报错：lv_sdl_private.h: 无法打开 "SDL2/SDL.h"）
    find_package(SDL2 REQUIRED)

    # 把 LVGL 作为子目录加入
    add_subdirectory(third_party/lvgl ${CMAKE_BINARY_DIR}/lvgl)
    target_link_libraries(motor_oop_demo PRIVATE lvgl)

    # 保险：显式把 SDL2 挂到 lvgl 目标上，确保编 LVGL 的 SDL 驱动时
    # 头文件目录里包含 SDL2/SDL.h（LVGL 用 lv_conf.h 而非 Kconfig 时，
    # 它自带的依赖解析有时抓不到 SDL2）
    target_link_libraries(lvgl PUBLIC SDL2::SDL2)

    # 把 SDL2 的运行时 DLL 拷到 exe 旁边（否则运行时找不到 SDL2.dll）
    add_custom_command(TARGET motor_oop_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL2::SDL2>"
                "$<TARGET_FILE_DIR:motor_oop_demo>"
        COMMENT "拷贝 SDL2.dll 到输出目录")
endif()
