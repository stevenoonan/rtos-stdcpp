# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
project(rtos_stdcpp_compile_check LANGUAGES C CXX)
set(FREERTOS_KERNEL_ROOT "" CACHE PATH "Externally supplied kernel source root")
set(FREERTOS_PORT_INCLUDE "" CACHE PATH "Matching kernel port include directory")
if(NOT EXISTS "${FREERTOS_KERNEL_ROOT}/include/FreeRTOS.h" OR
   NOT EXISTS "${FREERTOS_PORT_INCLUDE}/portmacro.h")
    message(FATAL_ERROR "Supply FREERTOS_KERNEL_ROOT and FREERTOS_PORT_INCLUDE; nothing is fetched automatically")
endif()
add_library(freertos_headers INTERFACE)
target_include_directories(freertos_headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${FREERTOS_KERNEL_ROOT}/include" "${FREERTOS_PORT_INCLUDE}")
set(RTOS_STDCPP_PROFILE "stm32-gcc-14.3.1-cxx17")
set(RTOS_STDCPP_FREERTOS_TARGET freertos_headers)
add_subdirectory(../.. library)
target_link_libraries(freertos_headers INTERFACE rtos-stdcpp::config)
add_library(public_api_check OBJECT public_api.cpp)
target_link_libraries(public_api_check PRIVATE rtos-stdcpp::rtos-stdcpp freertos_headers)
set_target_properties(public_api_check PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
