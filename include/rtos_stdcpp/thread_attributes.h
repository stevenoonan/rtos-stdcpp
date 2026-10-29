#pragma once

#ifdef RTOS_STDCPP_BACKEND_THREADX
#include "threadx_thread_attributes.h"
#else
#include "freertos_thread_attributes.h"
#endif
