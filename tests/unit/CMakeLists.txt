# SPDX-License-Identifier: MIT
foreach(rate IN ITEMS 100 1000)
    add_executable(time_algorithms_${rate} time_algorithms.cpp)
    target_include_directories(time_algorithms_${rate} PRIVATE
        stubs "${PROJECT_SOURCE_DIR}/src/backends/freertos")
    target_link_libraries(time_algorithms_${rate} PRIVATE rtos_stdcpp_test_support test_host_platform)
    target_compile_definitions(time_algorithms_${rate} PRIVATE configTICK_RATE_HZ=${rate})
    target_compile_options(time_algorithms_${rate} PRIVATE
        -Wall -Wextra -Werror)
    set_target_properties(time_algorithms_${rate} PROPERTIES
        CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME time_algorithms_${rate} COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/run_tests.py"
        --suite arithmetic --layer host --timeout 20 -- $<TARGET_FILE:time_algorithms_${rate}>)
    set_tests_properties(time_algorithms_${rate} PROPERTIES LABELS "host;arithmetic" TIMEOUT 25)
endforeach()

foreach(rate IN ITEMS 100 1000)
    add_executable(threadx_time_${rate} threadx_time.cpp)
    target_include_directories(threadx_time_${rate} PRIVATE threadx_time_stubs "${PROJECT_SOURCE_DIR}/src/backends/threadx")
    target_compile_definitions(threadx_time_${rate} PRIVATE TX_TIMER_TICKS_PER_SECOND=${rate})
    target_compile_options(threadx_time_${rate} PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(threadx_time_${rate} PRIVATE rtos_stdcpp_host_sanitizer)
    set_target_properties(threadx_time_${rate} PROPERTIES
        CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    add_test(NAME threadx_time_${rate} COMMAND threadx_time_${rate})
    set_tests_properties(threadx_time_${rate} PROPERTIES LABELS "host;arithmetic" TIMEOUT 10)
endforeach()

add_executable(thread_identity_tests thread_identity.cpp)
target_include_directories(thread_identity_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/common")
target_compile_options(thread_identity_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(thread_identity_tests PRIVATE rtos_stdcpp_host_sanitizer)
set_target_properties(thread_identity_tests PROPERTIES
    CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_test(NAME thread_identity COMMAND thread_identity_tests)
set_tests_properties(thread_identity PROPERTIES LABELS "host;identity" TIMEOUT 10)

add_executable(threadx_identity_tests threadx_identity.cpp
    "${PROJECT_SOURCE_DIR}/src/backends/threadx/lifecycle.cpp")
target_include_directories(threadx_identity_tests PRIVATE threadx_stubs
    "${PROJECT_SOURCE_DIR}/src/backends/threadx"
    "${PROJECT_SOURCE_DIR}/include/rtos_stdcpp")
target_compile_options(threadx_identity_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(threadx_identity_tests PRIVATE rtos_stdcpp_host_sanitizer)
target_link_options(threadx_identity_tests PRIVATE -Wl,--wrap=malloc,--wrap=free)
set_target_properties(threadx_identity_tests PROPERTIES
    CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_test(NAME threadx_identity COMMAND threadx_identity_tests)
set_tests_properties(threadx_identity PROPERTIES LABELS "host;identity" TIMEOUT 10)

add_executable(freertos_identity_tests freertos_identity.cpp
    "${PROJECT_SOURCE_DIR}/src/backends/freertos/lifecycle.cpp")
target_include_directories(freertos_identity_tests PRIVATE freertos_identity_stubs
    "${PROJECT_SOURCE_DIR}/src/backends/freertos"
    "${PROJECT_SOURCE_DIR}/include/rtos_stdcpp")
target_compile_options(freertos_identity_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(freertos_identity_tests PRIVATE rtos_stdcpp_host_sanitizer)
set_target_properties(freertos_identity_tests PROPERTIES
    CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_test(NAME freertos_identity COMMAND freertos_identity_tests)
set_tests_properties(freertos_identity PROPERTIES LABELS "host;identity" TIMEOUT 10)

if(RTOS_STDCPP_HOST_SANITIZER STREQUAL "address")
    add_executable(asan_probe asan_probe.cpp)
    target_link_libraries(asan_probe PRIVATE rtos_stdcpp_host_sanitizer)
    add_test(NAME asan_negative_control COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/check_asan.py"
        --probe $<TARGET_FILE:asan_probe>)
    set_tests_properties(asan_negative_control PROPERTIES LABELS "host;harness;asan" TIMEOUT 15)
endif()
