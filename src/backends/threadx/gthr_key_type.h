/// Copyright 2018-2023 Piotr Grygorczuk <grygorek@gmail.com>
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.

#ifndef __FREERTOS_GTHR_KEY_KEY_H__
#define __FREERTOS_GTHR_KEY_KEY_H__

#include "thread_gthread.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace rtos_stdcpp {

struct Key {
    using __gthread_t = rtos_stdcpp::gthr_threadx;
    typedef void (*destructor_function)(void*);

    Key() = delete;

    explicit Key(destructor_function des) : _destructor{des} {}

    void call_destructor(__gthread_t::native_task_type task) {
        void* val;

        {
            std::lock_guard<std::mutex> lg{_mtx};

            auto item{_specific_values.find(task)};
            if (item == _specific_values.end()) {
                return;
            }

            val = const_cast<void*>(item->second);
            _specific_values.erase(item);
        }

        if (_destructor && val) {
            _destructor(val);
        }
    }

    std::mutex _mtx;
    destructor_function _destructor;
    std::unordered_map<__gthread_t::native_task_type, const void*> _specific_values;
};

} // namespace rtos_stdcpp

#endif //__FREERTOS_GTHR_KEY_KEY_H__
