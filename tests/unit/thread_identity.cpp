// SPDX-License-Identifier: MIT
#include "thread_identity.h"
#include <array>
#include <cassert>
#include <cstring>
#include <type_traits>

using namespace rtos_stdcpp::internal;

int main() {
    static_assert(std::is_trivially_copyable_v<thread_identity>);
    static_assert(std::has_unique_object_representations_v<thread_identity>);
    static_assert(sizeof(thread_identity) == 2 * sizeof(uintptr_t));
    assert(thread_identity{} == thread_identity{});
    const thread_identity old{0x20001000, 17};
    const thread_identity replacement{0x20001000, 18};
    const thread_identity other_address{0x20002000, 17};
    assert(!(old == replacement));
    assert(!(old == other_address));
    assert(old < replacement);
    assert(old < other_address);
    const auto copy = old;
    assert(std::memcmp(&old, &copy, sizeof(old)) == 0);

    // Every seed maps above the agreed lower bound, including zero/all-ones.
    uint32_t seed = 0;
    for (unsigned i = 0; i < 100000; ++i) {
        assert(thread_id_registry::initial_id(seed) > 0x11111111U);
        seed = seed * 1664525U + 1013904223U;
    }
    assert(thread_id_registry::initial_id(UINT32_MAX) > 0x11111111U);
    assert(thread_id_registry::initial_id(0) != thread_id_registry::initial_id(1));

    thread_id_registry registry;
    thread_registration a{}, b{}, c{}, d{};
    assert(registry.insert(a, 123));
    const auto first = a.id;
    assert(first > 0x11111111U);
    assert(registry.insert(b, 999)); // Later seeds do not restart the sequence.
    assert(b.id == (first == UINT32_MAX ? 1U : first + 1));
    assert(registry.insert(c, 999));
    assert(registry.oldest == &a && registry.newest == &c);
    registry.erase(b);
    assert(a.next == &c && c.previous == &a);
    registry.erase(c);
    assert(registry.newest == &a && !a.next);

    // Model a full counter circuit without creating four billion workers.
    registry.next_id = a.id;
    const auto blocked_next = registry.next_id;
    assert(!registry.insert(d, 0));
    assert(d.id == 0 && registry.next_id == blocked_next);
    registry.erase(a);
    assert(!registry.oldest && !registry.newest);
    assert(registry.insert(d, 0));
    assert(d.id == blocked_next);
    registry.erase(d);

    registry.next_id = UINT32_MAX;
    assert(registry.insert(a, 0) && a.id == UINT32_MAX);
    assert(registry.insert(b, 0) && b.id == 1);
    assert(registry.insert(c, 0) && c.id == 2);
    assert(registry.oldest == &a); // Oldest is not numerically smallest.
    registry.erase(a);
    assert(registry.oldest == &b && !b.previous);
    registry.erase(c);
    registry.erase(b);

    // Reuse the same registration storage repeatedly and retain old identities.
    std::array<thread_identity, 128> saved{};
    for (auto& identity : saved) {
        assert(registry.insert(a, 0));
        identity = {reinterpret_cast<uintptr_t>(&a), a.id};
        registry.erase(a);
    }
    for (size_t i = 1; i < saved.size(); ++i) {
        assert(!(saved[i] == saved[i - 1]));
    }
}
