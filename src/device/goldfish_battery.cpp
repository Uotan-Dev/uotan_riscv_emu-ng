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

#include "device/goldfish_battery.hpp"

namespace uemu::device {

GoldfishBattery::GoldfishBattery(IrqCallback irq_callback,
                                 uint32_t interrupt_id, uint32_t init_capacity)
    : IrqDevice("GoldfishBattery", DEFAULT_BASE, SIZE, irq_callback,
                interrupt_id),
      int_status_(0), int_enable_(0), ac_online_(1),
      status_(POWER_SUPPLY_STATUS_CHARGING), health_(POWER_SUPPLY_HEALTH_GOOD),
      present_(1), capacity_(init_capacity) {}

std::optional<uint64_t>
GoldfishBattery::read_internal(addr_t offset, [[maybe_unused]] size_t size) {
    switch (offset) {
        case INT_STATUS: {
            uint32_t r = int_status_ & int_enable_;

            if (r) {
                update_irq(false);
                int_status_ = 0;
            }

            return r;
        }

        case INT_ENABLE: return int_enable_;
        case AC_ONLINE: return ac_online_;
        case STATUS: return status_;
        case HEALTH: return health_;
        case PRESENT: return present_;
        case CAPACITY: return capacity_;

        default: return 0;
    }

    return 0;
}

bool GoldfishBattery::write_internal(addr_t offset,
                                     [[maybe_unused]] size_t size,
                                     uint64_t value) {
    if (offset == INT_ENABLE)
        int_enable_ = value;

    return true;
}

} // namespace uemu::device
