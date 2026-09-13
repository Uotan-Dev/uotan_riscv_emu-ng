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

#include <memory>
#include <mutex>

#include "core/hart.hpp"
#include "device/device.hpp"
#include "time_source.hpp"

namespace uemu::device {

class Clint final : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x2000000;
    static constexpr size_t SIZE = 0x10000;            // 64KB
    static constexpr uint64_t DEFAULT_FREQ = 10000000; // 10 MHz
    static constexpr addr_t MSIP_OFFSET = 0x0;
    static constexpr addr_t MTIMECMP_OFFSET = 0x4000;
    static constexpr addr_t MTIME_OFFSET = 0xBFF8;

    Clint(std::shared_ptr<core::Hart> hart, uint64_t freq_hz = DEFAULT_FREQ,
          TimerMode timer_mode = TimerMode::Realtime);

    void tick() override;
    uint64_t get_mtime() noexcept;
    void advance_timer(uint64_t ticks) noexcept;
    [[nodiscard]] bool uses_deterministic_timer() const noexcept;

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    void update_interrupts(uint64_t mtime) noexcept;
    void handle_mtimecmp(uint64_t mtime) noexcept;
    void handle_stimecmp(uint64_t mtime) noexcept;

    std::shared_ptr<core::Hart> hart_;

    std::mutex clint_mutex_;
    uint64_t mtimecmp_;
    std::unique_ptr<TimeSource> time_source_;
};

} // namespace uemu::device
