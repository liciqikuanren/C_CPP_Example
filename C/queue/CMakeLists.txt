cmake_minimum_required(VERSION 3.15)

project(queue LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(queue STATIC
    queue.c
    queue.h
)
target_include_directories(queue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(queue_demo
    main.c
)
target_link_libraries(queue_demo PRIVATE queue)

if(MSVC)
    target_compile_options(queue PRIVATE /W4 /utf-8)
    target_compile_options(queue_demo PRIVATE /W4 /utf-8)
else()
    target_compile_options(queue PRIVATE -Wall -Wextra)
    target_compile_options(queue_demo PRIVATE -Wall -Wextra)
endif()
