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

#include <chrono>

#include "device/goldfish_rtc.hpp"

namespace uemu::device {

GoldfishRTC::GoldfishRTC(IrqCallback irq_callback, uint32_t interrupt_id)
    : IrqDevice("GoldfishRTC", DEFAULT_BASE, SIZE, irq_callback, interrupt_id),
      tick_offset_(0), alarm_next_(0), alarm_running_(0), irq_pending_(0),
      irq_enabled_(0), time_high_(0) {
    auto now = std::chrono::system_clock::now();
    auto unix_time = std::chrono::system_clock::to_time_t(now);
    tick_offset_ =
        static_cast<uint64_t>(unix_time) * 1000000000ULL - get_host_time_ns();
}

void GoldfishRTC::tick() {
    std::lock_guard<std::mutex> lock(goldfish_rtc_mutex_);

    if (alarm_running_ && get_count() >= alarm_next_)
        trigger_interrupt();
}

std::optional<uint64_t> GoldfishRTC::read_internal(addr_t offset, size_t size) {
    if (size == 8) {
        std::optional<uint64_t> lo = read_internal(offset, 4);

        if (lo == std::nullopt) [[unlikely]]
            return std::nullopt;

        std::optional<uint64_t> hi = read_internal(offset + 4, 4);

        if (hi == std::nullopt) [[unlikely]]
            return std::nullopt;

        return *lo | (*hi << 32);
    }

    if (size != 4) [[unlikely]]
        return std::nullopt;

    std::lock_guard<std::mutex> lock(goldfish_rtc_mutex_);

    switch (offset) {
        case TIME_LOW: {
            uint64_t count = get_count();
            time_high_ = count >> 32;
            return count & 0xFFFFFFFF;
        }
        case TIME_HIGH: return time_high_;
        case ALARM_LOW: return alarm_next_ & 0xFFFFFFFF;
        case ALARM_HIGH: return alarm_next_ >> 32;
        case IRQ_ENABLED: return irq_enabled_;
        case ALARM_STATUS: return alarm_running_;
        default: return std::nullopt;
    }

    return std::nullopt;
}

bool GoldfishRTC::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size == 8)
        return write_internal(offset, 4, value & 0xFFFFFFFF) &&
               write_internal(offset + 4, 4, value >> 32);

    if (size != 4)
        return false;

    value &= 0xFFFFFFFF;

    std::lock_guard<std::mutex> lock(goldfish_rtc_mutex_);

    switch (offset) {
        case TIME_LOW: {
            uint64_t current_tick = get_count();
            uint64_t new_tick = (current_tick & 0xFFFFFFFF00000000ULL) | value;
            tick_offset_ += new_tick - current_tick;
            return true;
        }
        case TIME_HIGH: {
            uint64_t current_tick = get_count();
            uint64_t new_tick =
                (current_tick & 0x00000000FFFFFFFFULL) | (value << 32);
            tick_offset_ += new_tick - current_tick;
            return true;
        }
        case ALARM_LOW:
            alarm_next_ = (alarm_next_ & 0xFFFFFFFF00000000ULL) | value;
            set_alarm();
            return true;
        case ALARM_HIGH:
            alarm_next_ = (alarm_next_ & 0x00000000FFFFFFFFULL) | (value << 32);
            return true;
        case IRQ_ENABLED:
            irq_enabled_ = value & 1;
            update_irq();
            return true;
        case CLEAR_ALARM: clear_alarm(); return true;
        case CLEAR_INTERRUPT:
            irq_pending_ = 0;
            update_irq();
            return true;
        default: return false;
    }

    return false;
}

uint64_t GoldfishRTC::get_count() const {
    return get_host_time_ns() + tick_offset_;
}

uint64_t GoldfishRTC::get_host_time_ns() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
        .count();
}

void GoldfishRTC::set_alarm() {
    if (alarm_next_ <= get_count()) {
        clear_alarm();
        trigger_interrupt();
    } else {
        alarm_running_ = 1;
    }
}

void GoldfishRTC::clear_alarm() { alarm_running_ = 0; }

void GoldfishRTC::trigger_interrupt() {
    alarm_running_ = 0;
    irq_pending_ = 1;
    update_irq();
}

void GoldfishRTC::update_irq() {
    bool level = (irq_pending_ && irq_enabled_) ? true : false;
    IrqDevice::update_irq(level);
}

} // namespace uemu::device
