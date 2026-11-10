// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <type_traits>

#if __cplusplus > 201703L
#include <compare>
#endif

namespace rtos_stdcpp::internal {

// GCC hashes the entire gthread handle, including its object representation.
// Two pointer-sized integers avoid padding on both the supported 32-bit targets
// and 64-bit host fixtures. The numeric component is always a uint32_t value.
struct thread_identity {
    uintptr_t address{};
    uintptr_t number{};

    constexpr bool operator==(thread_identity other) const noexcept {
        return address == other.address && number == other.number;
    }

    constexpr bool operator<(thread_identity other) const noexcept {
        return address < other.address || (address == other.address && number < other.number);
    }

#if __cplusplus > 201703L
    constexpr auto operator<=>(thread_identity other) const noexcept {
        if (auto order = address <=> other.address; order != 0) {
            return order;
        }
        return number <=> other.number;
    }
#endif
};

static_assert(sizeof(thread_identity) == 2 * sizeof(uintptr_t));
static_assert(std::is_trivially_copyable_v<thread_identity>);
static_assert(std::has_unique_object_representations_v<thread_identity>);

struct thread_registration {
    uint32_t id{};
    thread_registration* previous{};
    thread_registration* next{};
};

// Intrusive creation-order list, independent of the kernel's task/reaper lists.
// Every operation must run under the backend's critical section. Entries stay
// registered until their completion block's last owner/worker reference is gone.
struct thread_id_registry {
    thread_registration* oldest{};
    thread_registration* newest{};
    uint32_t next_id{};

    static uint32_t initial_id(uint32_t seed) noexcept {
        // Noncryptographic avalanche, followed by mapping above 0x11111111.
        // Address/tick seeds can repeat across boots; this is not entropy.
        seed ^= seed >> 16;
        seed *= 0x7feb352dU;
        seed ^= seed >> 15;
        seed *= 0x846ca68bU;
        seed ^= seed >> 16;
        constexpr uint32_t first = 0x11111112U;
        return first + seed % (UINT32_MAX - first + 1);
    }

    bool insert(thread_registration& entry, uint32_t seed) noexcept {
        if (!next_id) {
            next_id = initial_id(seed);
        }
        if (entry.id || (oldest && next_id == oldest->id)) {
            return false;
        }

        entry.id = next_id;
        next_id = next_id == UINT32_MAX ? 1 : next_id + 1;
        entry.previous = newest;
        entry.next = nullptr;
        if (newest) {
            newest->next = &entry;
        } else {
            oldest = &entry;
        }
        newest = &entry;
        return true;
    }

    void erase(thread_registration& entry) noexcept {
        if (entry.previous) {
            entry.previous->next = entry.next;
        } else {
            oldest = entry.next;
        }
        if (entry.next) {
            entry.next->previous = entry.previous;
        } else {
            newest = entry.previous;
        }
        entry = {};
    }
};

} // namespace rtos_stdcpp::internal
