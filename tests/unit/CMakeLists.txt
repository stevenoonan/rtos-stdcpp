# SPDX-License-Identifier: MIT
foreach(rate IN ITEMS 100 1000)
    add_executable(time_algorithms_${rate} time_algorithms.cpp)
    target_include_directories(time_algorithms_${rate} PRIVATE
        stubs "${PROJECT_SOURCE_DIR}/src/backends/freertos")
    target_compile_definitions(time_algorithms_${rate} PRIVATE configTICK_RATE_HZ=${rate})
    target_compile_options(time_algorithms_${rate} PRIVATE
        -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all)
    target_link_options(time_algorithms_${rate} PRIVATE -fsanitize=undefined)
    set_target_properties(time_algorithms_${rate} PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME time_algorithms_${rate} COMMAND time_algorithms_${rate})
    set_tests_properties(time_algorithms_${rate} PROPERTIES LABELS host TIMEOUT 20)
endforeach()
