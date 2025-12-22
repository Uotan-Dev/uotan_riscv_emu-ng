/*
 * Copyright 2025 Nuo Shen, Nanjing University
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

#include <algorithm>
#include <format>
#include <stdexcept>
#include <vector>

#include "core/dram.hpp"
#include "device/device.hpp"

namespace uemu::core {

class Bus {
public:
    explicit Bus(std::shared_ptr<Dram> dram) : dram_(std::move(dram)) {}

    // Register a memory-mapped device.
    // Device address range must not overlap DRAM or existing devices.
    void add_device(std::shared_ptr<device::Device> dev) {
        if (!dev)
            throw std::runtime_error("Bus: Attempted to add a null device");

        constexpr addr_t dram_start = Dram::DRAM_BASE;
        const addr_t dram_end = dram_start + dram_->size() - 1;

        if (check_overlap(dev->start(), dev->end(), dram_start, dram_end)) {
            throw std::runtime_error(std::format(
                "Bus: Device '{}' [{:#x}-{:#x}] overlaps with DRAM "
                "[{:#x}-{:#x}]",
                dev->name(), dev->start(), dev->end(), dram_start, dram_end));
        }

        for (const auto& existing : devices_) {
            if (check_overlap(dev->start(), dev->end(), existing->start(),
                              existing->end())) {
                throw std::runtime_error(std::format(
                    "Bus: Device '{}' [{:#x}-{:#x}] overlaps with existing "
                    "device '{}' [{:#x}-{:#x}]",
                    dev->name(), dev->start(), dev->end(), existing->name(),
                    existing->start(), existing->end()));
            }
        }

        devices_.push_back(std::move(dev));
    }

    // Read a value of type T from `addr`.
    // No permission/alignment checks are performed here.
    // Returns std::nullopt if no owner for the address is found.
    template <typename T>
    [[nodiscard]] std::optional<T> read(addr_t addr) noexcept {
        if (dram_->is_valid_addr(addr, sizeof(T))) [[likely]]
            return dram_->read<T>(addr);

        for (const auto& dev : devices_)
            if (dev->contains(addr, sizeof(T)))
                return dev->read<T>(addr);

        return std::nullopt;
    }

    // Write a value of type T to `addr`.
    // No permission/alignment checks are performed here.
    // Returns false if no owner for the address is found.
    template <typename T>
    [[nodiscard]] bool write(addr_t addr, T value) noexcept {
        if (dram_->is_valid_addr(addr, sizeof(T))) [[likely]] {
            dram_->write<T>(addr, value);
            return true;
        }

        for (const auto& dev : devices_) {
            if (dev->contains(addr, sizeof(T))) {
                dev->write<T>(addr, value);
                return true;
            }
        }

        return false;
    }

private:
    static inline bool check_overlap(addr_t s1, addr_t e1, addr_t s2,
                                     addr_t e2) {
        return std::max(s1, s2) <= std::min(e1, e2);
    }

    std::shared_ptr<Dram> dram_;
    std::vector<std::shared_ptr<device::Device>> devices_;
};

} // namespace uemu::core
