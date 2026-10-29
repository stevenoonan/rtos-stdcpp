# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
project(rtos_stdcpp_compile_check LANGUAGES C CXX)

set(RTOS_STDCPP_BACKEND "FreeRTOS" CACHE STRING "Compile fixture: FreeRTOS or ThreadX")
set_property(CACHE RTOS_STDCPP_BACKEND PROPERTY STRINGS FreeRTOS ThreadX)
set(RTOS_STDCPP_PROFILE "stm32-gcc-14.3.1-cxx17" CACHE STRING "Embedded profile")
set(FREERTOS_KERNEL_ROOT "" CACHE PATH "Externally supplied FreeRTOS source root")
set(FREERTOS_PORT_INCLUDE "" CACHE PATH "Matching FreeRTOS port include directory")
set(THREADX_ROOT "" CACHE PATH "Externally supplied ThreadX source root")

add_library(kernel_headers INTERFACE)
if(RTOS_STDCPP_BACKEND STREQUAL "FreeRTOS")
    if(NOT EXISTS "${FREERTOS_KERNEL_ROOT}/include/FreeRTOS.h" OR
       NOT EXISTS "${FREERTOS_PORT_INCLUDE}/portmacro.h")
        message(FATAL_ERROR "Supply FREERTOS_KERNEL_ROOT and FREERTOS_PORT_INCLUDE; nothing is fetched automatically")
    endif()
    target_include_directories(kernel_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/freertos" "${FREERTOS_KERNEL_ROOT}/include" "${FREERTOS_PORT_INCLUDE}")
    set(RTOS_STDCPP_FREERTOS_TARGET kernel_headers)
elseif(RTOS_STDCPP_BACKEND STREQUAL "ThreadX")
    if(NOT EXISTS "${THREADX_ROOT}/common/inc/tx_api.h" OR
       NOT EXISTS "${THREADX_ROOT}/ports/cortex_m55/gnu/inc/tx_port.h")
        message(FATAL_ERROR "Supply THREADX_ROOT with common and GNU Cortex-M55 headers; nothing is fetched automatically")
    endif()
    target_include_directories(kernel_headers INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/threadx" "${THREADX_ROOT}/common/inc"
        "${THREADX_ROOT}/ports/cortex_m55/gnu/inc")
    target_compile_definitions(kernel_headers INTERFACE TX_INCLUDE_USER_DEFINE_FILE)
    set(RTOS_STDCPP_THREADX_TARGET kernel_headers)
else()
    message(FATAL_ERROR "RTOS_STDCPP_BACKEND must be FreeRTOS or ThreadX")
endif()

add_subdirectory(../.. library)
target_link_libraries(kernel_headers INTERFACE rtos-stdcpp::config)

add_library(public_api_check OBJECT public_api.cpp)
target_link_libraries(public_api_check PRIVATE rtos-stdcpp::rtos-stdcpp kernel_headers)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/runtime.cmake")
rtos_stdcpp_add_runtime(compiler_runtime BACKEND "${RTOS_STDCPP_BACKEND}" CONFIG_TARGET kernel_headers)
if(RTOS_STDCPP_BACKEND STREQUAL "ThreadX")
    target_sources(public_api_check PRIVATE threadx/application.cpp)
endif()
get_target_property(standard rtos_stdcpp CXX_STANDARD)
set_target_properties(public_api_check compiler_runtime PROPERTIES
    CXX_STANDARD ${standard} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

enable_testing()
add_test(NAME embedded_compile COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
    --target public_api_check rtos_stdcpp compiler_runtime)
set_tests_properties(embedded_compile PROPERTIES LABELS compile_link TIMEOUT 90)
