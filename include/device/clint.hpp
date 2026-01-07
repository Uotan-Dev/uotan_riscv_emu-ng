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

#include <chrono>
#include <mutex>

#include "core/hart.hpp"
#include "device/device.hpp"

namespace uemu::device {

class Clint final : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x2000000;
    static constexpr size_t SIZE = 0x10000;            // 64KB
    static constexpr uint64_t DEFAULT_FREQ = 10000000; // 10 MHz
    static constexpr addr_t MSIP_OFFSET = 0x0;
    static constexpr addr_t MTIMECMP_OFFSET = 0x4000;
    static constexpr addr_t MTIME_OFFSET = 0xBFF8;

    Clint(std::shared_ptr<core::Hart> hart, uint64_t freq_hz = DEFAULT_FREQ);

    void tick() override;

private:
    uint64_t read_internal(addr_t offset, size_t size) override;
    void write_internal(addr_t offset, size_t size, uint64_t value) override;

    inline void handle_mtimecmp();
    inline void handle_stimecmp();
    inline void handle_time();

    std::shared_ptr<core::Hart> hart_;

    std::mutex clint_mutex_;
    uint64_t mtime_;
    uint64_t mtimecmp_;

    std::chrono::steady_clock::time_point start_time_;
    uint64_t freq_hz_;
};

} // namespace uemu::device
