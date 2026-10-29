// SPDX-License-Identifier: MIT
#include "tls_peer.h"
#include "runner.h"

#ifdef RTOS_STDCPP_TEST_EMBEDDED
extern "C" void __emutls_register_common(void*, uintptr_t, uintptr_t, void*);
extern "C" void* __emutls_get_address(void*);

namespace {
struct Control {
    uintptr_t size, alignment, index;
    void* initial;
};

unsigned small = 9, ignored = 99, full[] = {5, 6};
Control common{sizeof(small), alignof(unsigned), 0, &small};
Control common_zero{sizeof(small), alignof(unsigned), 0, &small};

struct Register {
    Register() {
        __emutls_register_common(&common, sizeof(full), 64, nullptr);
        __emutls_register_common(&common, sizeof(ignored), 4, &ignored);
        __emutls_register_common(&common, sizeof(full), 4, full);
        __emutls_register_common(&common_zero, sizeof(full), 128, nullptr);
        __emutls_register_common(&common_zero, sizeof(small), 4, &small);
    }
} registration;
} // namespace
#endif
namespace rtos_test::tls_test {
namespace {
volatile unsigned dynamic_seed = 43;

struct Dynamic {
    unsigned value;

    Dynamic() : value(dynamic_seed) {}

    ~Dynamic() {
        TEST_EQ(99U, value);
    }
};

thread_local Dynamic dynamic;
volatile bool touch_optional = false;

struct Untouched {
    Untouched() {
        TEST_REQUIRE(false);
    }

    ~Untouched() {
        TEST_REQUIRE(false);
    }
};

void optional(bool touch) {
    if (touch) {
        static thread_local Untouched value;
        (void)value;
    }
}
} // namespace
#ifdef RTOS_STDCPP_TEST_EMBEDDED
thread_local unsigned external = 53;

unsigned* external_address() {
    return &external;
}
#endif
unsigned* peer_address() {
    return &cross_unit;
}

void peer_lifetime(std::atomic<unsigned>& destroyed) {
    struct Object {
        std::atomic<unsigned>* count;

        ~Object() {
            ++*count;
        }
    };

    static thread_local Object object{&destroyed};
    (void)object;
}

void check_common() {
#ifdef RTOS_STDCPP_TEST_EMBEDDED
    TEST_EQ(sizeof(full), common.size);
    TEST_EQ(64U, common.alignment);
    auto* value = static_cast<unsigned*>(__emutls_get_address(&common));
    TEST_EQ(0U, reinterpret_cast<uintptr_t>(value) % 64);
    TEST_EQ(5U, value[0]);
    TEST_EQ(6U, value[1]);
    value[0] = 77;
    auto* zero = static_cast<unsigned*>(__emutls_get_address(&common_zero));
    TEST_EQ(0U, reinterpret_cast<uintptr_t>(zero) % 128);
    TEST_EQ(0U, zero[0]);
    TEST_EQ(0U, zero[1]);
    zero[0] = 88;
#endif
}

void check_dynamic() {
    TEST_EQ(43U, dynamic.value);
    dynamic.value = 99;
    optional(touch_optional);
}
} // namespace rtos_test::tls_test
