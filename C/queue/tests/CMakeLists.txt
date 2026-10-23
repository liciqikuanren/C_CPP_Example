add_library(unity STATIC
    third_party/unity/unity.c
)
target_include_directories(unity PUBLIC third_party/unity)

if(MSVC)
    target_compile_options(unity PRIVATE /W3 /utf-8)
else()
    target_compile_options(unity PRIVATE -Wall)
endif()

function(queue_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE queue unity)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

queue_add_test(test_queue)
