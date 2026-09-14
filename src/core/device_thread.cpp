/*
 * Copyright 2026 Nuo Shen, Nanjing University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdexcept>

#include "core/device_thread.hpp"

namespace uemu::core {

DeviceThread::DeviceThread(Bus& bus, std::stop_source stop_source)
    : bus_(bus), stop_source_(std::move(stop_source)) {}

DeviceThread::~DeviceThread() {
    stop_source_.request_stop();
    static_cast<void>(join());
}

void DeviceThread::start() {
    if (thread_.joinable())
        throw std::logic_error("DeviceThread::start() called twice");

    thread_ = std::thread(&DeviceThread::run, this);
}

std::exception_ptr DeviceThread::join() {
    if (thread_.joinable())
        thread_.join();

    return exception_;
}

void DeviceThread::run() {
    try {
        // Tick continuously, exactly like the main loop used to, so that
        // device progress and interrupt delivery keep their current latency.
        while (!stop_source_.stop_requested()) {
            bus_.tick_devices();
            std::this_thread::yield();
        }
    } catch (...) {
        exception_ = std::current_exception();
        stop_source_.request_stop();
    }
}

} // namespace uemu::core
