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
#include "ui/input_sink.hpp"

namespace uemu::device {

class GoldfishEvents : public IrqDevice, public ui::InputSink {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10002000;
    static constexpr size_t SIZE = 0x1000;
    static constexpr uint32_t DEFAULT_INTERRUPT_ID = 2;

    // Register offsets
    static constexpr addr_t REG_READ = 0x00;
    static constexpr addr_t REG_SET_PAGE = 0x00;
    static constexpr addr_t REG_LEN = 0x04;
    static constexpr addr_t REG_DATA = 0x08;

    // Pages
    static constexpr uint32_t PAGE_NAME = 0x00000;
    static constexpr uint32_t PAGE_EVBITS = 0x10000;
    static constexpr uint32_t PAGE_ABSDATA = 0x20000 | EV_ABS;

    // Maximum events in queue
    static constexpr size_t MAX_EVENTS = 256 * 4;

    GoldfishEvents(IrqCallback irq_callback,
                   uint32_t interrupt_id = DEFAULT_INTERRUPT_ID,
                   const std::string& device_name = "qwerty2");

    void push_key_event(KeyEvent event) override;

private:
    // Device state
    enum State : uint32_t {
        STATE_INIT = 0, // Device is initialized
        STATE_BUFFERED, // Events buffered, but no IRQ raised yet
        STATE_LIVE      // Events can be sent directly to kernel
    };

    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    // Event queue operations
    void enqueue_event(uint32_t type, uint32_t code, int value);
    uint32_t dequeue_event();

    // Event bits operations
    void set_event_bits(uint32_t type, size_t bitl, size_t bith);
    void set_event_bit(uint32_t type, size_t bit);

    // Page operations
    size_t get_page_len() const;
    uint8_t get_page_data(size_t offset) const;

    const std::string device_name_;

    mutable std::mutex goldfish_events_mutex_;
    uint32_t page_;
    State state_;
    std::array<uint32_t, MAX_EVENTS> events_;
    size_t first_, last_;

    struct EvBits {
        std::vector<uint8_t> bits;
    };

    std::array<EvBits, EV_MAX + 1> ev_bits_;
};

} // namespace uemu::device
