/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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

#include <functional>

#include "device/device.hpp"

namespace uemu::device {

class SiFiveTest : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x100000;
    static constexpr size_t SIZE = 0x1000;

    enum Status : uint16_t { FAIL = 0x3333, PASS = 0x5555, RESET = 0x7777 };

    using ShutdownCallback = std::function<void(int, Status)>;

    SiFiveTest(ShutdownCallback on_shutdown)
        : Device("SiFiveTest", DEFAULT_BASE, SIZE), on_shutdown_(on_shutdown) {}

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    ShutdownCallback on_shutdown_;
};

} // namespace uemu::device
