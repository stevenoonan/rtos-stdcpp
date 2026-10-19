# SPDX-License-Identifier: MIT
function(rtos_stdcpp_check_profile)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
       NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "14.3.1")
        message(FATAL_ERROR "The selected profile requires the audited GNU Tools for STM32 GCC 14.3.1")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../tools/check_profile.py"
        --profile "${RTOS_STDCPP_PROFILE}" --compiler "${CMAKE_CXX_COMPILER}" --c-compiler "${CMAKE_C_COMPILER}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "rtos-stdcpp profile verification failed:\n${output}${error}")
    endif()
    message(STATUS "${output}")
endfunction()
