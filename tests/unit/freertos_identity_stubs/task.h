// SPDX-License-Identifier: MIT
#pragma once
#include "FreeRTOS.h"

struct FakeTask {
    void* context{};
    void (*entry)(void*){};
    void* argument{};
};

using TaskHandle_t = FakeTask*;
BaseType_t xTaskGetSchedulerState();
void vTaskSuspendAll();
BaseType_t xTaskResumeAll();
BaseType_t xTaskCreate(void (*)(void*), const char*, configSTACK_DEPTH_TYPE, void*, UBaseType_t,
                       TaskHandle_t*);
TaskHandle_t xTaskGetCurrentTaskHandle();
void* pvTaskGetThreadLocalStoragePointer(TaskHandle_t, BaseType_t);
void vTaskSetThreadLocalStoragePointer(TaskHandle_t, BaseType_t, void*);
void vTaskDelete(TaskHandle_t);
TickType_t xTaskGetTickCount();
