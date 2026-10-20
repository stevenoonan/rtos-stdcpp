// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>

namespace free_rtos_std::internal
{
struct tls_destructor;

// Owned by the worker, inside its stable completion block. No allocation is
// needed until a language TLS variable is actually accessed.
struct tls_context
{
  void **values;
  std::size_t capacity;
  tls_destructor *destructors;

  void run_destructors() noexcept;
  void release_storage() noexcept;
};
}
