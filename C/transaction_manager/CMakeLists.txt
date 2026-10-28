cmake_minimum_required(VERSION 3.16)
project(transaction_manager C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# 事务管理器静态库
add_library(txn_manager STATIC src/txn_manager.c)
target_include_directories(txn_manager PUBLIC inc)
# 演示程序把槽位表缩小到 4（也演示了该参数可配置）
target_compile_definitions(txn_manager PUBLIC TXN_MAX_SLOTS=4)

# 演示程序
add_executable(transaction_manager_demo main.c)
target_link_libraries(transaction_manager_demo PRIVATE txn_manager)

if(MSVC)
    # /utf-8 让 MSVC 按 UTF-8 读取含中文注释的源文件
    target_compile_options(txn_manager PUBLIC /W4 /utf-8)
else()
    target_compile_options(txn_manager PUBLIC -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME demo COMMAND transaction_manager_demo)
