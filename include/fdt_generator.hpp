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

#include <cstdint>
#include <vector>

#include "board_config.hpp"

namespace uemu {

// Produces the firmware description of a BoardConfig: the same values Emulator
// turns into runtime devices, written as a flattened device tree.
//
// The tree is what firmware sees, so every value here is guest-visible machine
// ABI; it describes only what the guest can discover, which is why devices
// without a standard binding (the NEMU debug console, the ACT-only interrupt
// generator) are deliberately absent, as in the board layout README documents.
//
// The generator only reads the configuration; constructing devices, choosing a
// DRAM address for the blob and handing it over in a1 remain Emulator's job.
class FdtGenerator {
public:
    // `config` must outlive the generator; it is not copied.
    explicit FdtGenerator(const BoardConfig& config) : config_(config) {}

    [[nodiscard]] std::vector<uint8_t> generate() const;

private:
    const BoardConfig& config_;
};

} // namespace uemu
