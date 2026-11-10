// SPDX-License-Identifier: MIT
#include "console.h"
#include "platform.h"
#include "runner.h"
#include "FreeRTOS.h"
#include "task.h"
#include <condition_variable>
#include <chrono>
#include <cstdlib>
using namespace std::chrono_literals;

int main() {
    using namespace rtos_test;
    Report report{};
    reset(report, Suite::freertos);
    begin(report, Case::condition_notification_isolation);
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false, done = false;
    auto child = platform::worker(1, [&] {
        xTaskNotifyGiveIndexed(xTaskGetCurrentTaskHandle(), 1);
        std::unique_lock<std::mutex> held(mutex);
        ready = true;
        cv.notify_one();
        TEST_REQUIRE(cv.wait_for(held, 100ms, [&] {
            return done;
        }));
        TEST_EQ(1U, ulTaskNotifyTakeIndexed(1, pdTRUE, 0));
    });
    {
        std::unique_lock<std::mutex> held(mutex);
        TEST_REQUIRE(cv.wait_for(held, 100ms, [&] {
            return ready;
        }));
        done = true;
        cv.notify_one();
    }
    child.join();
    end(report, Case::condition_notification_isolation);
    const bool pass = finish(report);
    write_report(report, [](const char* text) {
        print(text);
    });
    std::_Exit(pass ? 0 : 1);
}
