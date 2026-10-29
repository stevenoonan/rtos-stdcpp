// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Run from a scheduled task. Requires an idle opportunity for deletion cleanup.
uint32_t freertos_cpp11_lifecycle_regression(void);
#ifdef __cplusplus
}
#endif
