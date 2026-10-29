#include "thread_gthread.h"
#include <exception>

namespace rtos_stdcpp {
internal::tls_context& gthr_freertos::thread_storage() {
    // Check before querying a current TCB, including when assertions are disabled.
    if (backend_interrupt_context() || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        std::terminate();
    }
    auto* shared = static_cast<completion*>(pvTaskGetThreadLocalStoragePointer(nullptr, tls_slot));
    if (!shared) {
        std::terminate();
    }
    return shared->tls;
}
} // namespace rtos_stdcpp
