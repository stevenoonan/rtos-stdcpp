# SPDX-License-Identifier: MIT
function(rtos_stdcpp_check_profile)
    # The bundled libstdc++ providers use private interfaces from this release.
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
       NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
       NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL "14.3.1" OR
       NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "14.3.1")
        message(FATAL_ERROR "rtos-stdcpp requires GCC 14.3.1 for both C and C++")
    endif()
endfunction()
