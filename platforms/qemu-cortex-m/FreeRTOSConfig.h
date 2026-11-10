// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#define configENABLE_TRUSTZONE 0
#define configRUN_FREERTOS_SECURE_ONLY 1
#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define configENABLE_MVE 0
#define configENABLE_PAC 0
#define configENABLE_BTI 0
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE 0
#define configCPU_CLOCK_HZ RTOS_STDCPP_CPU_HZ
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 512
#define configTOTAL_HEAP_SIZE (1024 * 1024)
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_NEWLIB_REENTRANT 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 1
#define configUSE_TIMERS 0
#define configUSE_IDLE_HOOK 1
#define configUSE_TICK_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x80
#define configKERNEL_INTERRUPT_PRIORITY 0xff
#define configCHECK_HANDLER_INSTALLATION 1
#define configASSERT(value) do { if (!(value)) rtos_test_assert_fail(__FILE__, __LINE__); } while (0)
void rtos_test_assert_fail(const char*, unsigned);
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_xTaskAbortDelay 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
