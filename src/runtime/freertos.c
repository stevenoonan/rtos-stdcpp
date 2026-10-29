#include "rtos_stdcpp/runtime.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static StaticSemaphore_t guard_storage;
static SemaphoreHandle_t guard_mutex;

void rtos_stdcpp_runtime_initialize(void) {}

void rtos_stdcpp_runtime_lock(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(!rtos_stdcpp_runtime_is_interrupt());
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskSuspendAll();
    }
}

void rtos_stdcpp_runtime_unlock(void) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        (void)xTaskResumeAll();
    }
}

void rtos_stdcpp_guard_lock(void) {
    RTOS_STDCPP_RUNTIME_REQUIRE(!rtos_stdcpp_runtime_is_interrupt());
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }
    taskENTER_CRITICAL();
    if (!guard_mutex) {
        guard_mutex = xSemaphoreCreateRecursiveMutexStatic(&guard_storage);
    }
    taskEXIT_CRITICAL();
    RTOS_STDCPP_RUNTIME_REQUIRE(guard_mutex != NULL);
    RTOS_STDCPP_RUNTIME_REQUIRE(xSemaphoreTakeRecursive(guard_mutex, portMAX_DELAY) == pdTRUE);
}

void rtos_stdcpp_guard_unlock(void) {
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        RTOS_STDCPP_RUNTIME_REQUIRE(xSemaphoreGiveRecursive(guard_mutex) == pdTRUE);
    }
}
