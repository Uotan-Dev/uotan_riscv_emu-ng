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

#include <iostream>

#include "device/device.hpp"

namespace uemu::device {

class NemuConsole : public Device {
public:
    static constexpr size_t DEFAULT_BASE = 0x10008000;
    static constexpr size_t SIZE = 8;

    NemuConsole(std::ostream& out = std::cout)
        : Device("NemuConsole", DEFAULT_BASE, SIZE), out_(out) {}

private:
    uint64_t read_internal(addr_t addr, size_t size) override;
    void write_internal(addr_t addr, size_t size, uint64_t value) override;

    std::ostream& out_;
};

} // namespace uemu::device
