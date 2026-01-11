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

#include <limits>

#include "device/clint.hpp"

namespace uemu::device {

Clint::Clint(std::shared_ptr<core::Hart> hart, uint64_t freq_hz)
    : Device("CLINT", DEFAULT_BASE, SIZE), hart_(std::move(hart)), mtime_(0),
      mtimecmp_(std::numeric_limits<reg_t>::max()), freq_hz_(freq_hz) {
    start_time_ = std::chrono::steady_clock::now();
}

void Clint::tick() {
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time_;
        std::lock_guard<std::mutex> lock(clint_mutex_);
        mtime_ = static_cast<uint64_t>(elapsed.count() * freq_hz_);
        handle_time();
        handle_mtimecmp();
        handle_stimecmp();
    }
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
        // MTIMECMP
        uint64_t result = 0;
        std::lock_guard<std::mutex> lock(clint_mutex_);
        read_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, &result);
        return result;
    } else if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME
        uint64_t result = 0;
        std::lock_guard<std::mutex> lock(clint_mutex_);
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
        // MTIMECMP
        std::lock_guard<std::mutex> lock(clint_mutex_);
        write_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, value);

        handle_time();
        handle_mtimecmp();
        handle_stimecmp();
    } else if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME
        std::lock_guard<std::mutex> lock(clint_mutex_);
        write_little_endian(&mtime_, offset - MTIME_OFFSET, size, value);

        handle_time();

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> new_elapsed(static_cast<double>(mtime_) /
                                                  freq_hz_);
        start_time_ =
            now -
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                new_elapsed);

        handle_mtimecmp();
        handle_stimecmp();
    } else {
        return false;
    }

    return true;
}

void Clint::handle_mtimecmp() {
    hart_->set_interrupt_pending(core::MIP::Field::MTIP, mtime_ >= mtimecmp_);
}

void Clint::handle_stimecmp() {
    core::MENVCFG* menvcfg =
        dynamic_cast<core::MENVCFG*>(hart_->csrs[core::MENVCFG::ADDRESS].get());
    core::STIMECMP* stimecmp = dynamic_cast<core::STIMECMP*>(
        hart_->csrs[core::STIMECMP::ADDRESS].get());
    assert(menvcfg && stimecmp);

    if (menvcfg->read_unchecked() & core::MENVCFG::Field::STCE)
        hart_->set_interrupt_pending(core::MIP::Field::STIP,
                                     mtime_ >= stimecmp->read_unchecked());
}

void Clint::handle_time() {
    core::TIME* time =
        dynamic_cast<core::TIME*>(hart_->csrs[core::TIME::ADDRESS].get());
    assert(time);

    time->mirror_from_mtime(mtime_);
}

}; // namespace uemu::device
