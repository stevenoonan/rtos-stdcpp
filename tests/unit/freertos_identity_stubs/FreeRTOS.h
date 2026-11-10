// SPDX-License-Identifier: MIT
#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using configSTACK_DEPTH_TYPE = uint32_t;
constexpr BaseType_t pdPASS = 1, pdFALSE = 0, pdTRUE = 1;
constexpr BaseType_t taskSCHEDULER_NOT_STARTED = 0;
constexpr UBaseType_t tskIDLE_PRIORITY = 0;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
#define configASSERT(value) assert(value)
void enter_critical();
void exit_critical();
#define taskENTER_CRITICAL() enter_critical()
#define taskEXIT_CRITICAL() exit_critical()
void* pvPortMalloc(size_t);
void vPortFree(void*);
