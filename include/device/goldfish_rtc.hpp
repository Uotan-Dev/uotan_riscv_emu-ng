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

#include <mutex>

#include "device/device.hpp"

namespace uemu::device {

class GoldfishRTC : public IrqDevice {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10000100;
    static constexpr size_t SIZE = 0x100;
    static constexpr uint32_t DEFAULT_INTERRUPT_ID = 11;

    // Register offsets
    static constexpr addr_t TIME_LOW = 0x00;
    static constexpr addr_t TIME_HIGH = 0x04;
    static constexpr addr_t ALARM_LOW = 0x08;
    static constexpr addr_t ALARM_HIGH = 0x0c;
    static constexpr addr_t IRQ_ENABLED = 0x10;
    static constexpr addr_t CLEAR_ALARM = 0x14;
    static constexpr addr_t ALARM_STATUS = 0x18;
    static constexpr addr_t CLEAR_INTERRUPT = 0x1c;

    GoldfishRTC(IrqCallback irq_callback,
                uint32_t interrupt_id = DEFAULT_INTERRUPT_ID);

    void tick() override;

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    // Get current emulated time in nanoseconds
    uint64_t get_count() const;

    // Get host monotonic time in nanoseconds
    static uint64_t get_host_time_ns();

    // Alarm management
    void set_alarm();
    void clear_alarm();
    void trigger_interrupt();
    void update_irq();

    std::mutex goldfish_rtc_mutex_;

    // Offset between host time and guest time (in nanoseconds)
    uint64_t tick_offset_;

    // The time for the next alarm (in guest nanoseconds)
    uint64_t alarm_next_;

    // Flags
    uint32_t alarm_running_; // Flag indicating if an alarm is set
    uint32_t irq_pending_;   // Flag indicating if an interrupt is pending
    uint32_t irq_enabled_;   // Flag for the interrupt enable register

    // Latched high 32 bits of the current time (for atomic 64-bit reads)
    uint32_t time_high_;
};

} // namespace uemu::device
