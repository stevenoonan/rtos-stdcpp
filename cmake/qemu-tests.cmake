# SPDX-License-Identifier: MIT
# Shared image/test registration; the machine supplies kernel, platform, linker
# script, optional runtime objects, CPU profile and the FP assembly probe.
function(rtos_stdcpp_qemu_images)
    function(image name)
        add_executable(${name} ${ARGN})
        set_target_properties(${name} PROPERTIES SUFFIX .elf)
        target_link_libraries(${name} PRIVATE platform kernel ${runtime_objects}
            rtos-stdcpp::rtos-stdcpp rtos-stdcpp::test-support)
        target_link_options(${name} PRIVATE --specs=nosys.specs -nostartfiles
            "-T${linker_script}" "-Wl,--gc-sections,-Map,${CMAKE_CURRENT_BINARY_DIR}/${name}.map")
        set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS "${linker_script}")
    endfunction()
    target_include_directories(platform PUBLIC "${root}/tests/qualification")
    target_compile_definitions(platform PUBLIC
        RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR=$<BOOL:${RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR}>
        RTOS_STDCPP_TEST_TICK_WRAP=$<BOOL:${test_tick_wrap}>)
    if(RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR)
        target_sources(platform PRIVATE "${root}/tests/qualification/tls_allocator.cpp")
    endif()
    image(conformance "${root}/tests/support/embedded_main.cpp")
    image(qualification "${root}/tests/qualification/main.cpp" "${fpu_probe}")
    image(lifecycle "${root}/tests/regression/lifecycle_regression.cpp" "${root}/tests/regression/main.cpp")
    target_include_directories(lifecycle PRIVATE "${root}/tests/regression")
    if(RTOS_STDCPP_CXX_STANDARD EQUAL 20)
        image(cxx20 "${root}/tests/support/embedded_main.cpp")
        target_compile_definitions(cxx20 PRIVATE RTOS_STDCPP_RUN_CXX20=1)
    endif()
    set(failure_modes throwing_new joinable recursive_guard tls_startup tls_native tls_alloc tls_overflow)
    if(RTOS_STDCPP_BACKEND STREQUAL "FreeRTOS")
        list(APPEND failure_modes kernel_alloc)
        image(backend "${root}/tests/qualification/freertos.cpp")
    endif()
    foreach(mode IN LISTS failure_modes)
        image(fail_${mode} "${root}/tests/qualification/failure.cpp")
        string(TOUPPER "${mode}" definition)
        target_compile_definitions(fail_${mode} PRIVATE FAIL_${definition}=1 FAILURE_NAME="${mode}")
    endforeach()
    set(RTOS_STDCPP_QEMU "" CACHE FILEPATH "Pinned QEMU executable")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(RTOS_STDCPP_QEMU)
        # Required suites share the same image names on every machine/backend.
        foreach(name IN ITEMS conformance qualification lifecycle cxx20 backend)
            if(NOT TARGET ${name})
                continue()
            endif()
            set(suite ${name})
            if(name STREQUAL "conformance")
                set(suite cxx17)
            elseif(name STREQUAL "lifecycle")
                set(suite regression)
            elseif(name STREQUAL "backend")
                set(suite freertos)
            endif()
            set(deadline 25)
            if(name STREQUAL "qualification")
                set(deadline 90)
            endif()
            add_test(NAME ${name} COMMAND "${Python3_EXECUTABLE}" "${root}/tools/run_qemu.py"
                --qemu "${RTOS_STDCPP_QEMU}" --machine ${qemu_machine} --backend ${RTOS_STDCPP_BACKEND}
                --image $<TARGET_FILE:${name}> --suite ${suite} --timeout ${deadline}
                --output "${CMAKE_CURRENT_BINARY_DIR}/${name}.json")
            math(EXPR outer_deadline "${deadline} + 10")
            set_tests_properties(${name} PROPERTIES LABELS emulator TIMEOUT ${outer_deadline})
        endforeach()
        set(allocator_arguments)
        if(RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR)
            list(APPEND allocator_arguments --custom-tls-allocator)
        endif()
        foreach(mode IN LISTS failure_modes)
            add_test(NAME fail_${mode} COMMAND "${Python3_EXECUTABLE}" "${root}/tools/run_qemu.py"
                --qemu "${RTOS_STDCPP_QEMU}" --machine ${qemu_machine} --backend ${RTOS_STDCPP_BACKEND}
                --image $<TARGET_FILE:fail_${mode}> --failure ${mode} ${allocator_arguments}
                --output "${CMAKE_CURRENT_BINARY_DIR}/fail_${mode}.json")
            set_tests_properties(fail_${mode} PROPERTIES LABELS emulator TIMEOUT 35)
        endforeach()
        add_test(NAME runtime_link COMMAND "${Python3_EXECUTABLE}" "${root}/tools/check_qemu.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --toolchain "${RTOS_STDCPP_TOOLCHAIN_ROOT}"
            --machine ${qemu_machine} --backend ${RTOS_STDCPP_BACKEND})
        set_tests_properties(runtime_link PROPERTIES LABELS compile_link TIMEOUT 20)
    endif()
endfunction()
