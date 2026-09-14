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

#pragma once

#include <exception>
#include <stop_token>
#include <thread>

#include "core/bus.hpp"

namespace uemu::core {

// Owns the device thread, which advances every memory-mapped device.  Devices
// keep their own state and locking; this class only decides when they run.
class DeviceThread {
public:
    DeviceThread(Bus& bus, std::stop_source stop_source);

    ~DeviceThread();

    DeviceThread(const DeviceThread&) = delete;
    DeviceThread& operator=(const DeviceThread&) = delete;
    DeviceThread(DeviceThread&&) = delete;
    DeviceThread& operator=(DeviceThread&&) = delete;

    // Starts the device thread.  Throws std::logic_error when called twice.
    void start();

    // Joins the device thread when it was started.
    void join();

    [[nodiscard]] std::exception_ptr exception() const noexcept {
        return exception_;
    }

private:
    void run();

    Bus& bus_;
    std::stop_source stop_source_;

    std::thread thread_;
    std::exception_ptr exception_;
};

} // namespace uemu::core
