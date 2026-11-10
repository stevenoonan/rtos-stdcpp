// SPDX-License-Identifier: MIT
#pragma once
#include "FreeRTOS.h"
using EventBits_t = uint32_t;

struct FakeEvent {
    EventBits_t flags{};
};

using EventGroupHandle_t = FakeEvent*;
EventGroupHandle_t xEventGroupCreate();
void vEventGroupDelete(EventGroupHandle_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t, BaseType_t,
                                TickType_t);
