// SPDX-License-Identifier: MIT
// Compile-only fixture for the qualified Cortex-M55 port; no scheduler is run.
#pragma once
#include <stdint.h>
#define configENABLE_TRUSTZONE 0
#define configRUN_FREERTOS_SECURE_ONLY 1
#define configENABLE_FPU 1
#define configENABLE_MPU 0
#define configENABLE_MVE 0
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ 800000000UL
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 16
#define configMINIMAL_STACK_SIZE 256
#define configTOTAL_HEAP_SIZE 131072
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_NEWLIB_REENTRANT 1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 2
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 256
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x50
#define configKERNEL_INTERRUPT_PRIORITY 0xf0
#define configASSERT(value) do { if (!(value)) __builtin_trap(); } while (0)
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_xTaskAbortDelay 1
