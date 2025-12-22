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

#include "core/bus.hpp"
#include "core/hart.hpp"

namespace uemu::core {

class MMU {
public:
    explicit MMU(std::shared_ptr<Hart> hart, std::shared_ptr<Bus> bus)
        : hart_(std::move(hart)), bus_(std::move(bus)) {}

    // Read a value of type T from `addr` during instruction execution.
    // Only valid in the instruction execution stage; may throw Trap.
    template <typename T>
    [[nodiscard]] T read(addr_t pc, addr_t addr) {
        std::optional<T> v = bus_->read<T>(addr);

        if (!v.has_value()) [[unlikely]]
            Trap::raise_exception(pc, Exception::LoadAccessFault, addr);

        return *v;
    }

    // Write a value of type T to `addr` during instruction execution.
    // Only valid in the instruction execution stage; may throw Trap.
    template <typename T>
    void write(addr_t pc, addr_t addr, T value) {
        bool res = bus_->write<T>(addr, value);

        if (!res) [[unlikely]]
            Trap::raise_exception(pc, Exception::StoreAMOAccessFault, addr);
    }

    // Fetch a 32-bit instruction from the current PC.
    // Only valid in the instruction fetch stage; may throw Trap.
    [[nodiscard]] uint32_t ifetch() {
        std::optional<uint32_t> v = bus_->read<uint32_t>(hart_->pc);

        if (!v.has_value()) [[unlikely]]
            Trap::raise_exception(hart_->pc, Exception::InstructionAccessFault,
                                  hart_->pc);

        return *v;
    }

private:
    std::shared_ptr<Hart> hart_;
    std::shared_ptr<Bus> bus_;
};

} // namespace uemu::core
