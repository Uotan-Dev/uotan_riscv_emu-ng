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

#include "device/device.hpp"

namespace uemu::device {

class GoldfishBattery : public IrqDevice {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10003000;
    static constexpr size_t SIZE = 0x1000;
    static constexpr uint32_t DEFAULT_INTERRUPT_ID = 3;

    // Register offsets
    static constexpr addr_t INT_STATUS = 0x00;
    static constexpr addr_t INT_ENABLE = 0x04;
    static constexpr addr_t AC_ONLINE = 0x08;
    static constexpr addr_t STATUS = 0x0C;
    static constexpr addr_t HEALTH = 0x10;
    static constexpr addr_t PRESENT = 0x14;
    static constexpr addr_t CAPACITY = 0x18;

    static constexpr uint32_t BATTERY_STATUS_CHANGED = 1u << 0;
    static constexpr uint32_t AC_STATUS_CHANGED = 1u << 1;
    static constexpr uint32_t BATTERY_INT_MASK =
        BATTERY_STATUS_CHANGED | AC_STATUS_CHANGED;

    // Power supply
    static constexpr int POWER_SUPPLY_STATUS_CHARGING = 1;
    static constexpr int POWER_SUPPLY_HEALTH_GOOD = 1;

    GoldfishBattery(IrqCallback irq_callback,
                    uint32_t interrupt_id = DEFAULT_INTERRUPT_ID,
                    uint32_t init_capacity = 96);

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    uint32_t int_status_; // IRQs
    uint32_t int_enable_; // irq enable mask for int_status_
    uint32_t ac_online_;
    uint32_t status_;
    uint32_t health_;
    uint32_t present_;
    uint32_t capacity_;
};

} // namespace uemu::device
