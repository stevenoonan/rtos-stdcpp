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
project(rtos-stdcpp VERSION 0.1.0 LANGUAGES C CXX)

option(RTOS_STDCPP_BUILD_LIBRARY "Build the embedded adapter" ON)
option(RTOS_STDCPP_BUILD_HOST_TESTS "Build standalone arithmetic tests" OFF)
set(RTOS_STDCPP_PROFILE "" CACHE STRING "Exact qualified compiler/runtime profile")
set(RTOS_STDCPP_FREERTOS_TARGET "" CACHE STRING "Existing target supplying FreeRTOS configuration/includes")

if(RTOS_STDCPP_BUILD_LIBRARY)
    if(NOT RTOS_STDCPP_PROFILE STREQUAL "stm32-gcc-14.3.1-cxx17")
        message(FATAL_ERROR "Select RTOS_STDCPP_PROFILE=stm32-gcc-14.3.1-cxx17; no other embedded profile is qualified")
    endif()
    if(NOT RTOS_STDCPP_FREERTOS_TARGET OR NOT TARGET "${RTOS_STDCPP_FREERTOS_TARGET}")
        message(FATAL_ERROR "RTOS_STDCPP_FREERTOS_TARGET must name an existing target supplying FreeRTOS headers/configuration")
    endif()
    include(cmake/profiles/stm32-gcc-14.3.1-cxx17.cmake)
    rtos_stdcpp_check_profile()

    # Apply this object-free interface to ALL affected C/C++ targets, including
    # the kernel. It has no dependency on the supplied FreeRTOS target.
    add_library(rtos_stdcpp_config INTERFACE)
    add_library(rtos-stdcpp::config ALIAS rtos_stdcpp_config)
    target_include_directories(rtos_stdcpp_config BEFORE INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/toolchains/gcc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/backends/freertos"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/rtos_stdcpp")
    target_compile_features(rtos_stdcpp_config INTERFACE cxx_std_17)
    target_compile_definitions(rtos_stdcpp_config INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_HAS_GTHREADS=1>)
    target_compile_options(rtos_stdcpp_config INTERFACE
        $<$<COMPILE_LANGUAGE:C,CXX>:-mthumb;-mcpu=cortex-m55;-mfpu=fpv5-d16;-mfloat-abi=hard;-mcmse;--specs=nano.specs>
        $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/toolchains/gcc/gcc_threading_config.h;-fthreadsafe-statics;-fno-exceptions;-fno-rtti;-Werror=c++20-extensions>)

    set(runtime "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libstdcxx/${RTOS_STDCPP_PROFILE}/libstdc++-v3/src/c++11")
    # OBJECT is intentional: providers must be linked even if an archive could
    # otherwise satisfy a reference with the stock single-thread implementation.
    add_library(rtos_stdcpp OBJECT
        src/backends/freertos/thread.cpp
        src/backends/freertos/gthr_key.cpp
        src/backends/freertos/freertos_time.cpp
        "${runtime}/mutex.cc" "${runtime}/condition_variable.cc")
    add_library(rtos-stdcpp::rtos-stdcpp ALIAS rtos_stdcpp)
    target_link_libraries(rtos_stdcpp PUBLIC rtos_stdcpp_config
        PRIVATE "${RTOS_STDCPP_FREERTOS_TARGET}")
    set_target_properties(rtos_stdcpp PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
endif()

if(RTOS_STDCPP_BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(tests/unit)
endif()
