# 事件总线模块：静态库 + 示例程序
cmake_minimum_required(VERSION 3.16)
project(event_bus C)

# C99 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 编译告警：MSVC 用 /W4，其余编译器用 -Wall -Wextra
# /utf-8：源码为 UTF-8 中文注释，指定 MSVC 按 UTF-8 解析，避免 C4819 代码页告警
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 事件总线静态库
add_library(event_bus STATIC
    event_bus.c
)
target_include_directories(event_bus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 示例可执行程序，链接静态库
add_executable(event_bus_demo main.c)
target_link_libraries(event_bus_demo PRIVATE event_bus)

# 测试：直接运行示例程序
enable_testing()
add_test(NAME event_bus_demo COMMAND event_bus_demo)
