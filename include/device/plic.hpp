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
 *
 * ---
 * This file is part of uemu-ng and contains code derived from the Spike
 * (riscv-isa-sim) project.
 *
 * Spike is Copyright (c) 2010-2017, The Regents of the University of
 * California. Spike's original code is licensed under the BSD 3-Clause License.
 * Full Spike license can be found at:
 * https://opensource.org/licenses/BSD-3-Clause
 */

#pragma once

#include <mutex>
#include <vector>

#include "core/hart.hpp"
#include "device/device.hpp"

namespace uemu::device {

class Plic final : public Device {
public:
    static constexpr addr_t DEFAULT_BASE = 0xC000000;
    static constexpr size_t SIZE = 0x1000000;

    // Each interrupt source has a priority register associated with it.
    static constexpr addr_t PRIORITY_BASE = 0;
    static constexpr size_t PRIORITY_PER_ID = 4;

    // Each interrupt source has a pending bit associated with it.
    static constexpr addr_t PENDING_BASE = 0x1000;

    // Each hart context has a vector of interrupt enable bits associated with
    // it. There's one bit for each interrupt source.
    static constexpr addr_t ENABLE_BASE = 0x2000;
    static constexpr size_t ENABLE_PER_HART = 0x80;

    // Each hart context has a set of control registers associated with it.
    // Right now there's only two: a source priority threshold over which the
    // hart will take an interrupt, and a register to claim interrupts.
    static constexpr addr_t CONTEXT_BASE = 0x200000;
    static constexpr size_t CONTEXT_PER_HART = 0x1000;
    static constexpr addr_t CONTEXT_THRESHOLD = 0;
    static constexpr addr_t CONTEXT_CLAIM = 4;

    static constexpr size_t MAX_DEVICES = 1024;
    static constexpr size_t MAX_CONTEXTS = 15872;

    static constexpr size_t PRIO_BITS = 4;

    Plic(std::shared_ptr<core::Hart> hart, uint32_t ndev = 31);

    void set_interrupt_level(uint32_t id, bool lvl);

private:
    struct Context {
        Context(core::Hart* hart, bool mmode) : hart_(hart), mmode_(mmode) {}

        core::Hart* hart_;
        bool mmode_;

        uint8_t priority_threshold{};
        uint32_t enable[MAX_DEVICES / 32]{};
        uint32_t pending[MAX_DEVICES / 32]{};
        uint8_t pending_priority[MAX_DEVICES]{};
        uint32_t claimed[MAX_DEVICES / 32]{};
    };

    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    uint32_t context_best_pending(const Context* ctx);
    void context_update(const Context* ctx);
    uint32_t context_claim(Context* ctx);
    uint32_t priority_read(reg_t offset);
    void priority_write(reg_t offset, uint32_t val);
    uint32_t pending_read(reg_t offset);
    uint32_t context_enable_read(const Context* ctx, reg_t offset);
    void context_enable_write(Context* ctx, reg_t offset, uint32_t val);
    uint32_t context_read(Context* ctx, reg_t offset);
    bool context_write(Context* ctx, reg_t offset, uint32_t val);

    std::shared_ptr<core::Hart> hart_;

    std::mutex plic_mutex_;

    std::vector<Context> contexts_;
    uint32_t num_ids_;
    uint32_t num_ids_word_;
    uint32_t max_prio_;
    uint8_t priority_[MAX_DEVICES];
    uint32_t level_[MAX_DEVICES / 32];
};

} // namespace uemu::device
