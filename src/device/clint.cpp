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

Clint::Clint(std::shared_ptr<core::Hart> hart, uint64_t freq_hz,
             TimerMode timer_mode)
    : Device("CLINT", DEFAULT_BASE, SIZE), hart_(std::move(hart)), mtimecmp_(0),
      time_source_(make_time_source(timer_mode, freq_hz)) {
    hart_->set_clint(this);
    tick();
}

void Clint::tick() {
    std::scoped_lock lock(clint_mutex_);
    update_interrupts(time_source_->read());
}

uint64_t Clint::get_mtime() noexcept {
    std::scoped_lock lock(clint_mutex_);
    const uint64_t mtime = time_source_->read();
    update_interrupts(mtime);
    return mtime;
}

void Clint::advance_timer(uint64_t ticks) noexcept {
    if (ticks == 0 || !time_source_->is_deterministic())
        return;

    std::scoped_lock lock(clint_mutex_);
    time_source_->advance(ticks);
    update_interrupts(time_source_->read());
}

bool Clint::uses_deterministic_timer() const noexcept {
    return time_source_->is_deterministic();
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
    }

    if (offset >= MTIMECMP_OFFSET && offset < MTIMECMP_OFFSET + 8) {
        // MTIMECMP
        uint64_t result = 0;
        std::scoped_lock lock(clint_mutex_);
        read_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, &result);
        return result;
    }

    if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME
        uint64_t cur_mtime = get_mtime();
        uint64_t result = 0;
        read_little_endian(&cur_mtime, offset - MTIME_OFFSET, size, &result);
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
        std::scoped_lock lock(clint_mutex_);
        write_little_endian(&mtimecmp_, offset - MTIMECMP_OFFSET, size, value);
        update_interrupts(time_source_->read());
    } else if (offset >= MTIME_OFFSET && offset < MTIME_OFFSET + 8) {
        // MTIME
        std::scoped_lock lock(clint_mutex_);
        uint64_t mtime = time_source_->read();
        write_little_endian(&mtime, offset - MTIME_OFFSET, size, value);
        time_source_->write(mtime);
        update_interrupts(mtime);
    } else {
        return false;
    }

    return true;
}

void Clint::update_interrupts(uint64_t mtime) noexcept {
    handle_mtimecmp(mtime);
    handle_stimecmp(mtime);
}

void Clint::handle_mtimecmp(uint64_t mtime) noexcept {
    hart_->set_interrupt_pending(core::MIP::Field::MTIP, mtime >= mtimecmp_);
}

void Clint::handle_stimecmp(uint64_t mtime) noexcept {
    core::MENVCFG* menvcfg =
        dynamic_cast<core::MENVCFG*>(hart_->csrs[core::MENVCFG::ADDRESS].get());
    core::STIMECMP* stimecmp = dynamic_cast<core::STIMECMP*>(
        hart_->csrs[core::STIMECMP::ADDRESS].get());
    assert(menvcfg && stimecmp);

    if (menvcfg->read_unchecked() & core::MENVCFG::Field::STCE)
        hart_->set_interrupt_pending(core::MIP::Field::STIP,
                                     mtime >= stimecmp->read_unchecked());
}

} // namespace uemu::device
