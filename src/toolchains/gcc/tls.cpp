// SPDX-License-Identifier: MIT
// GCC emulated TLS ABI for the qualified, statically linked ARM profiles.
// Keep the control layout compatible with GCC's libgcc/emutls.c. These are
// independent providers: the stock archives were built without our gthreads.
#include "thread_gthread.h"
#include <rtos_stdcpp/tls_allocator.h>
#include <cstring>
#include <exception>
#include <limits>

namespace
{
struct emutls_control
{
  uintptr_t size;
  uintptr_t alignment;
  union { uintptr_t index; void *pointer; } location;
  void *initial_value;
};
static_assert(sizeof(emutls_control) == 4 * sizeof(void *));
static_assert(sizeof(uintptr_t) == sizeof(unsigned int));
uintptr_t last_index;

bool interrupt_context()
{
  uint32_t status;
#if __ARM_ARCH_PROFILE == 'M'
  __asm volatile("mrs %0, ipsr" : "=r"(status));
  return status != 0;
#elif __ARM_ARCH_PROFILE == 'A'
  __asm volatile("mrs %0, cpsr" : "=r"(status));
  auto mode = status & 0x1f;
  return mode != 0x10 && mode != 0x1f; // Only User/System are task modes.
#else
#error "Language TLS requires a qualified ARM M or A profile"
#endif
}

constexpr auto maximum = std::numeric_limits<std::size_t>::max();

void *allocate(std::size_t size)
{
#if defined(RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR) && RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
  auto *storage = rtos_stdcpp_tls_allocate(size);
#else
  auto *storage = pvPortMalloc(size);
#endif
  if (!storage) std::terminate();
  return storage;
}

void deallocate(void *storage) noexcept
{
  if (!storage) return;
#if defined(RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR) && RTOS_STDCPP_TLS_CUSTOM_ALLOCATOR
  rtos_stdcpp_tls_deallocate(storage);
#else
  vPortFree(storage);
#endif
}

void grow(free_rtos_std::internal::tls_context &context, std::size_t index)
{
  if (index <= context.capacity) return;
  auto capacity = context.capacity ? context.capacity : 8;
  while (capacity < index)
  {
    if (capacity > maximum / 2) std::terminate();
    capacity *= 2;
  }
  if (capacity > maximum / sizeof(void *)) std::terminate();
  auto **values = static_cast<void **>(allocate(capacity * sizeof(void *)));
  std::memset(values, 0, capacity * sizeof(void *));
  if (context.capacity)
    std::memcpy(values, context.values, context.capacity * sizeof(void *));
  deallocate(context.values);
  context.values = values;
  context.capacity = capacity;
}

void *allocate_value(const emutls_control &control)
{
  auto alignment = control.alignment;
  if (!alignment || (alignment & (alignment - 1))) std::terminate();
  if (alignment < alignof(void *)) alignment = alignof(void *);
  // Store the original allocation immediately before the aligned object.
  if (alignment - 1 > maximum - sizeof(void *)) std::terminate();
  auto overhead = alignment - 1 + sizeof(void *);
  if (control.size > maximum - overhead) std::terminate();
  auto *raw = allocate(control.size + overhead);
  auto address = (reinterpret_cast<uintptr_t>(raw) + sizeof(void *) + alignment - 1)
                 & ~(alignment - 1);
  auto *value = reinterpret_cast<void *>(address);
  reinterpret_cast<void **>(value)[-1] = raw;
  if (control.initial_value)
    std::memcpy(value, control.initial_value, control.size);
  else
    std::memset(value, 0, control.size);
  return value;
}
}

namespace free_rtos_std
{
internal::tls_context &gthr_freertos::thread_storage()
{
  // Check before querying a current TCB, including when assertions are disabled.
  if (interrupt_context() || xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    std::terminate();
  auto *shared = static_cast<completion *>(
      pvTaskGetThreadLocalStoragePointer(nullptr, tls_slot));
  if (!shared) std::terminate();
  return shared->tls;
}

namespace internal
{
struct tls_destructor
{
  void (*function)(void *);
  void *object;
  tls_destructor *next;
};

void tls_context::run_destructors() noexcept
{
  while (destructors)
  {
    auto *entry = destructors;
    destructors = entry->next;
    auto function = entry->function;
    auto *object = entry->object;
    deallocate(entry);
    // Pop before calling: a destructor can initialize another TLS object.
    function(object);
  }
}

void tls_context::release_storage() noexcept
{
  for (std::size_t i = 0; i < capacity; ++i)
    if (values[i]) deallocate(reinterpret_cast<void **>(values[i])[-1]);
  deallocate(values);
  values = nullptr;
  capacity = 0;
}
}
}

extern "C" void *__emutls_get_address(void *descriptor)
{
  auto &context = free_rtos_std::gthr_freertos::thread_storage();
  auto &control = *static_cast<emutls_control *>(descriptor);
  auto index = __atomic_load_n(&control.location.index, __ATOMIC_ACQUIRE);
  if (!index)
  {
    free_rtos_std::critical_section guard;
    index = __atomic_load_n(&control.location.index, __ATOMIC_RELAXED);
    if (!index)
    {
      if (last_index == maximum) std::terminate();
      index = ++last_index;
      __atomic_store_n(&control.location.index, index, __ATOMIC_RELEASE);
    }
  }
  grow(context, index);
  auto *&value = context.values[index - 1];
  if (!value) value = allocate_value(control);
  return value;
}

extern "C" void __emutls_register_common(void *descriptor, uintptr_t size,
                                         uintptr_t alignment, void *initial_value)
{
  // GCC emits these registrations in startup constructors, before TLS access.
  auto &control = *static_cast<emutls_control *>(descriptor);
  if (control.size < size)
  {
    control.size = size;
    control.initial_value = nullptr;
  }
  if (control.alignment < alignment) control.alignment = alignment;
  if (initial_value && size == control.size) control.initial_value = initial_value;
}

extern "C" int __cxa_thread_atexit(void (*function)(void *), void *object,
                                  void * /* dso_handle */) noexcept
{
  // Images are statically linked; no DSO reference counting is necessary.
  auto &context = free_rtos_std::gthr_freertos::thread_storage();
  using free_rtos_std::internal::tls_destructor;
  auto *entry = ::new (allocate(sizeof(tls_destructor)))
      tls_destructor{function, object, context.destructors};
  context.destructors = entry;
  return 0;
}
