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

#include "core/hart.hpp"
#include "device/device.hpp"

namespace uemu::device {

class Clint final : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0x2000000;
    static constexpr size_t SIZE = 0x10000; // 64KB

    // Match Spike's instruction-count driven mtime: INSNS_PER_RTC_TICK = 100,
    // INTERLEAVE = 5000 (credit batch for round-robin WFI spin).
    static constexpr uint64_t INSNS_PER_RTC_TICK = 100;
    static constexpr uint64_t INTERLEAVE = 5000;

    static constexpr addr_t MSIP_OFFSET = 0x0;
    static constexpr addr_t MTIMECMP_OFFSET = 0x4000;
    static constexpr addr_t MTIME_OFFSET = 0xBFF8;

    explicit Clint(std::shared_ptr<core::Hart> hart);

    // Device interface — no-op: mtime is instruction-count driven, not
    // wall-clock.
    void tick() override {}

    // Return current mtime (no side effects). Called from TIME CSR read path.
    uint64_t get_mtime() const noexcept { return mtime_; }

    // Advance cycle_count_ by 1. Every INSNS_PER_RTC_TICK calls advance mtime_
    // by 1 tick and re-evaluate timer comparators.
    void update() noexcept;

    // Re-evaluate timer comparators immediately without advancing cycle_count_.
    // Called on STIMECMP CSR writes and MTIMECMP/MTIME MMIO writes so that
    // MTIP/STIP are updated without double-counting the cycle.
    void sync() noexcept;

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    void handle_mtimecmp() noexcept;
    void handle_stimecmp() noexcept;

    std::shared_ptr<core::Hart> hart_;

    uint64_t cycle_count_;
    uint64_t mtime_;
    uint64_t mtimecmp_;
};

} // namespace uemu::device
