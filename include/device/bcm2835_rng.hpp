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

#include <random>

#include "device/device.hpp"

namespace uemu::device {

class BCM2835Rng : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10004000;
    static constexpr size_t SIZE = 0x10;

    // Register offsets
    static constexpr addr_t RNG_CTRL = 0x0;
    static constexpr addr_t RNG_STATUS = 0x4;
    static constexpr addr_t RNG_DATA = 0x8;

    BCM2835Rng()
        : Device("BCM2835Rng", DEFAULT_BASE, SIZE), rd_(), gen_(rd_()),
          rng_ctrl_(0), rng_status_(0) {}

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    uint32_t get_random_bytes() const { return gen_(); }

    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    uint32_t rng_ctrl_;
    uint32_t rng_status_;
};

} // namespace uemu::device
