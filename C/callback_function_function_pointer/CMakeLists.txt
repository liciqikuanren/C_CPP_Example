cmake_minimum_required(VERSION 3.15)

project(callback_function_function_pointer LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(EXAMPLE_TARGETS
    01_function_pointer_basic
    02_callback_basic
    03_callback_register
    04_callback_qsort
)

foreach(target IN LISTS EXAMPLE_TARGETS)
    add_executable(${target} ${target}.c)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endforeach()
