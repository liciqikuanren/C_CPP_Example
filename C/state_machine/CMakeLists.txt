cmake_minimum_required(VERSION 3.16)
project(state_machine C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(state_machine STATIC src/state_machine.c)
target_include_directories(state_machine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(state_machine_demo src/main.c)
target_link_libraries(state_machine_demo PRIVATE state_machine)

find_package(Threads REQUIRED)

add_executable(state_machine_counter_demo src/counter_demo.c)
target_link_libraries(state_machine_counter_demo PRIVATE state_machine Threads::Threads)

if(MSVC)
    target_compile_options(state_machine PRIVATE /W4 /utf-8)
    target_compile_options(state_machine_demo PRIVATE /W4 /utf-8)
    target_compile_options(state_machine_counter_demo PRIVATE /W4 /utf-8)
else()
    target_compile_options(state_machine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(state_machine_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(state_machine_counter_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
