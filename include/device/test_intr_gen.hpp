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

#include <memory>

#include "core/hart.hpp"
#include "device/device.hpp"

namespace uemu::device {

// Simple Interrupt Generator (SIG) — aligned with Sail-RISCV's
// simple_interrupt_generator. Provides memory-mapped control for
// setting/clearing external and software interrupts in ACT compliance tests.
class TestIntrGen final : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x40000000;
    static constexpr size_t SIZE = 0x1000;

    // Register offsets (match Sail's SIG_VERSION_OFFSET / SIG_PLATFORM_OFFSET).
    static constexpr addr_t VERSION_OFFSET = 0x0;
    static constexpr addr_t PLATFORM_OFFSET = 0x4;

    // Sail SIG version (semver 1.0.0).
    static constexpr uint32_t SIG_VERSION = 0x00010000;

    // Platform register bit 31 acts as the set (1) / clear (0) flag.
    static constexpr uint32_t SET_FLAG = 1U << 31;

    // Interrupt bits in the platform register map 1:1 to MIP bits.
    static constexpr reg_t MEI_BIT = core::MIP::Field::MEIP;
    static constexpr reg_t SEI_BIT = core::MIP::Field::SEIP;
    static constexpr reg_t MSI_BIT = core::MIP::Field::MSIP;
    static constexpr reg_t SSI_BIT = core::MIP::Field::SSIP;

    // Mask of all supported interrupt bits.
    static constexpr reg_t INTR_MASK = MEI_BIT | SEI_BIT | MSI_BIT | SSI_BIT;

    explicit TestIntrGen(std::shared_ptr<core::Hart> hart);

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    std::shared_ptr<core::Hart> hart_;
};

} // namespace uemu::device
