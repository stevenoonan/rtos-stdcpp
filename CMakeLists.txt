# Copyright 2018-2025 Piotr Grygorczuk <grygorek@gmail.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rtos-stdcpp contributors
cmake_minimum_required(VERSION 3.22)
project(rtos-stdcpp VERSION 1.0.0 LANGUAGES C CXX)

option(RTOS_STDCPP_BUILD_LIBRARY "Build the embedded adapter" ON)
option(RTOS_STDCPP_BUILD_HOST_TESTS "Build standalone host tests and runner controls" OFF)
option(RTOS_STDCPP_BUILD_TEST_SUPPORT "Build reusable embedded test cases and report support" OFF)
option(RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR "Use application-provided TLS allocation/deallocation hooks" OFF)
set(RTOS_STDCPP_PROFILE "" CACHE STRING "Exact qualified compiler/runtime profile")
set(RTOS_STDCPP_BACKEND "FreeRTOS" CACHE STRING "RTOS backend: FreeRTOS or ThreadX")
set_property(CACHE RTOS_STDCPP_BACKEND PROPERTY STRINGS FreeRTOS ThreadX)
set(RTOS_STDCPP_THREADX_TARGET "" CACHE STRING "Target supplying ThreadX configuration/includes")
set(RTOS_STDCPP_FREERTOS_TARGET "" CACHE STRING "Existing target supplying FreeRTOS configuration/includes")

if(RTOS_STDCPP_BUILD_LIBRARY AND RTOS_STDCPP_PROFILE MATCHES "-cxx(17|20)$")
    set(profile_standard "${CMAKE_MATCH_1}")
    if(DEFINED RTOS_STDCPP_CXX_STANDARD AND NOT RTOS_STDCPP_CXX_STANDARD STREQUAL profile_standard)
        message(FATAL_ERROR "RTOS_STDCPP_CXX_STANDARD conflicts with RTOS_STDCPP_PROFILE")
    endif()
    set(RTOS_STDCPP_CXX_STANDARD "${profile_standard}")
elseif(NOT DEFINED RTOS_STDCPP_CXX_STANDARD)
    set(RTOS_STDCPP_CXX_STANDARD 17)
endif()
if(NOT RTOS_STDCPP_CXX_STANDARD MATCHES "^(17|20)$")
    message(FATAL_ERROR "RTOS_STDCPP_CXX_STANDARD must be 17 or 20")
endif()
set(CMAKE_CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(RTOS_STDCPP_BUILD_LIBRARY)
    set(qualified_profiles stm32-gcc-14.3.1-cxx17 qemu-a9-gcc-14.3.1-cxx17
        stm32-gcc-14.3.1-cxx20 qemu-a9-gcc-14.3.1-cxx20)
    if(NOT RTOS_STDCPP_PROFILE IN_LIST qualified_profiles)
        message(FATAL_ERROR "Select an exact qualified RTOS_STDCPP_PROFILE (${qualified_profiles})")
    endif()
    if(RTOS_STDCPP_BACKEND STREQUAL "ThreadX")
        if(NOT RTOS_STDCPP_PROFILE MATCHES "^stm32-")
            message(FATAL_ERROR "ThreadX currently supports the STM32 Cortex-M55 profiles")
        endif()
        set(backend threadx)
        set(backend_target "${RTOS_STDCPP_THREADX_TARGET}")
        set(backend_sources src/backends/threadx/lifecycle.cpp src/backends/threadx/threadx_time.cpp)
    elseif(RTOS_STDCPP_BACKEND STREQUAL "FreeRTOS")
        set(backend freertos)
        set(backend_target "${RTOS_STDCPP_FREERTOS_TARGET}")
        set(backend_sources src/backends/freertos/tls_context.cpp src/backends/freertos/freertos_time.cpp)
    else()
        message(FATAL_ERROR "RTOS_STDCPP_BACKEND must be FreeRTOS or ThreadX")
    endif()
    if(NOT backend_target OR NOT TARGET "${backend_target}")
        message(FATAL_ERROR "RTOS_STDCPP_${RTOS_STDCPP_BACKEND}_TARGET must name an existing configuration target")
    endif()
    include(cmake/profiles/stm32-gcc-14.3.1-cxx17.cmake)
    rtos_stdcpp_check_profile()

    # Apply this object-free interface to ALL affected C/C++ targets, including
    # the kernel. It has no dependency on the supplied FreeRTOS target.
    add_library(rtos_stdcpp_config INTERFACE)
    add_library(rtos-stdcpp::config ALIAS rtos_stdcpp_config)
    target_include_directories(rtos_stdcpp_config BEFORE INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/toolchains/gcc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backends/${backend}"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/rtos_stdcpp")
    if(backend STREQUAL "threadx")
        target_compile_definitions(rtos_stdcpp_config INTERFACE RTOS_STDCPP_BACKEND_THREADX=1)
    endif()
    target_compile_features(rtos_stdcpp_config INTERFACE cxx_std_${RTOS_STDCPP_CXX_STANDARD})
    target_compile_definitions(rtos_stdcpp_config INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_HAS_GTHREADS=1>)
    string(REPLACE "-cxx20" "-cxx17" identity_profile "${RTOS_STDCPP_PROFILE}")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/profiles/${identity_profile}.json" profile_json)
    if(RTOS_STDCPP_CXX_STANDARD EQUAL 20)
        target_include_directories(rtos_stdcpp_config BEFORE INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libstdcxx/stm32-gcc-14.3.1-cxx20")
        if(RTOS_STDCPP_BUILD_TEST_SUPPORT)
            target_compile_definitions(rtos_stdcpp_config INTERFACE
                $<$<COMPILE_LANGUAGE:CXX>:RTOS_STDCPP_TEST_STOP_LOCK=1>)
        endif()
    endif()
    string(JSON flag_count LENGTH "${profile_json}" cpu_flags)
    math(EXPR flag_last "${flag_count} - 1")
    set(cpu_flags)
    foreach(index RANGE ${flag_last})
        string(JSON flag GET "${profile_json}" cpu_flags ${index})
        list(APPEND cpu_flags "${flag}")
    endforeach()
    target_compile_options(rtos_stdcpp_config INTERFACE
        $<$<COMPILE_LANGUAGE:C,CXX>:${cpu_flags};--specs=nano.specs>
        $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/toolchains/gcc/gcc_threading_config.h;-fthreadsafe-statics;-fno-exceptions;-fno-rtti;-Werror=c++20-extensions>)
    target_link_options(rtos_stdcpp_config INTERFACE --specs=nano.specs)

    set(runtime "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libstdcxx/stm32-gcc-14.3.1-cxx17/libstdc++-v3/src/c++11")
    # OBJECT is intentional: providers must be linked even if an archive could
    # otherwise satisfy a reference with the stock single-thread implementation.
    add_library(rtos_stdcpp OBJECT
        src/backends/${backend}/thread.cpp
        src/backends/${backend}/gthr_key.cpp
        ${backend_sources}
        src/toolchains/gcc/tls.cpp
        "${runtime}/mutex.cc" "${runtime}/condition_variable.cc" "${runtime}/future.cc")
    add_library(rtos-stdcpp::rtos-stdcpp ALIAS rtos_stdcpp)
    if(RTOS_STDCPP_BUILD_TEST_SUPPORT)
        target_compile_definitions(rtos_stdcpp PRIVATE RTOS_STDCPP_TEST_CLOCK=1)
    endif()
    target_compile_definitions(rtos_stdcpp PRIVATE
        RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=$<BOOL:${RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR}>)
    target_link_libraries(rtos_stdcpp PUBLIC rtos_stdcpp_config
        PRIVATE "${backend_target}")
    set_target_properties(rtos_stdcpp PROPERTIES
        CXX_STANDARD ${RTOS_STDCPP_CXX_STANDARD} CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
endif()

if(RTOS_STDCPP_BUILD_HOST_TESTS)
    enable_testing()
endif()
if(RTOS_STDCPP_BUILD_HOST_TESTS OR RTOS_STDCPP_BUILD_TEST_SUPPORT)
    add_subdirectory(tests)
endif()
