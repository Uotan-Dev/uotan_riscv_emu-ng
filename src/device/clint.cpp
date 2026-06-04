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

#include "device/clint.hpp"

namespace uemu::device {

Clint::Clint(std::shared_ptr<core::Hart> hart)
    : Device("CLINT", DEFAULT_BASE, SIZE), hart_(std::move(hart)),
      cycle_count_(0), mtime_(0), mtimecmp_(0) {
    hart_->set_clint(this);
}

void Clint::update() noexcept {
    cycle_count_++;

    // At each INSNS_PER_RTC_TICK boundary: advance mtime by 1 tick and
    // re-evaluate timer comparators. Between boundaries, only cycle_count_
    // changes — no atomic MIP operations on the hot path.
    if (cycle_count_ % INSNS_PER_RTC_TICK == 0) [[unlikely]] {
        mtime_++;
        handle_mtimecmp();
        handle_stimecmp();
    }
}

void Clint::sync() noexcept {
    handle_mtimecmp();
    handle_stimecmp();
}

std::optional<uint64_t> Clint::read_internal(addr_t offset, size_t size) {
    if (size > 8) [[unlikely]]
        return std::nullopt;

    if (offset >= MSIP_OFFSET && offset < MSIP_OFFSET + 4) {
        // MSIP
        uint64_t msip_val = (hart_->csrs[core::MIP::ADDRESS]->read_unchecked() &
                             core::MIP::Field::MSIP)
                                ? 1
                                : 0;
        uint64_t result = 0;
        read_little_endian(&msip_val, offset - MSIP_OFFSET, size, &result);
        return result;
    } else if (offset >= MTIMECMP_OFFSET && offset < MTIMECMP_OFFSET + 8) {
        // MTIMECMP — no lock needed (CPU-thread only)
        uint64_t result = 0;
        read_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, &result);
        return result;
    } else if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME — no lock needed (CPU-thread only)
        uint64_t result = 0;
        read_little_endian(&mtime_, offset - MTIME_OFFSET, size, &result);
        return result;
    }

    return std::nullopt;
}

bool Clint::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (offset >= MSIP_OFFSET && offset < MSIP_OFFSET + 4) {
        // MSIP
        uint64_t msip_val = 0;
        write_little_endian(&msip_val, offset - MSIP_OFFSET, size, value);
        hart_->set_interrupt_pending(core::MIP::Field::MSIP, (msip_val & 1));
    } else if (offset >= MTIMECMP_OFFSET && offset < MTIMECMP_OFFSET + 8) {
        // MTIMECMP: store new value, immediately re-evaluate comparators
        // so MTIP is updated without waiting for the next update() boundary.
        write_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, value);
        handle_mtimecmp();
        handle_stimecmp();
    } else if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME: update mtime_ and resync cycle_count_ to maintain the
        // invariant mtime_ = cycle_count_ / INSNS_PER_RTC_TICK. The
        // sub-tick remainder is dropped (acceptable — this is a one-time
        // event per MTIME MMIO write).
        write_little_endian(&mtime_, offset - MTIME_OFFSET, size, value);
        cycle_count_ = mtime_ * INSNS_PER_RTC_TICK;
        handle_mtimecmp();
        handle_stimecmp();
    } else {
        return false;
    }

    return true;
}

void Clint::handle_mtimecmp() noexcept {
    hart_->set_interrupt_pending(core::MIP::Field::MTIP, mtime_ >= mtimecmp_);
}

void Clint::handle_stimecmp() noexcept {
    core::MENVCFG* menvcfg =
        dynamic_cast<core::MENVCFG*>(hart_->csrs[core::MENVCFG::ADDRESS].get());
    core::STIMECMP* stimecmp = dynamic_cast<core::STIMECMP*>(
        hart_->csrs[core::STIMECMP::ADDRESS].get());
    assert(menvcfg && stimecmp);

    if (menvcfg->read_unchecked() & core::MENVCFG::Field::STCE)
        hart_->set_interrupt_pending(core::MIP::Field::STIP,
                                     mtime_ >= stimecmp->read_unchecked());
}

} // namespace uemu::device
