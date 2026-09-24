#指定CMake的最低版本要求
cmake_minimum_required(VERSION 3.10)

#指定项目名称和版本
project(cmake_test VERSION 1.0)

#设置C标准为C11，并且要求必须使用C11标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

#添加全局头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)


#创建库目标Mylib（源文件为 src/mylib.cpp）
add_library(Mylib src/mylib.c)

#创建可执行文件目标MyExecutable（源文件为 src/main.c）
add_executable(MyExecutable src/main.c
    src/mylib2.c)

#将Mylib库链接到MyExecutable可执行文件
target_link_libraries(MyExecutable PRIVATE Mylib)


#同一个程序内部模块互相调用：都放进同一个 add_executable。
#多个程序或多个库都要用同一份代码：把它做成 add_library，然后 target_link_libraries。
#add_executable 生成最终程序，不是给别的目标复用的；add_library 生成的库才是给别的目标复用的。