cmake_minimum_required(VERSION 3.15)

# 项目信息
project(hello_world VERSION 1.0.0 DESCRIPTION "C++ Hello World 示例")

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 可执行文件
add_executable(hello_world main.cpp)
