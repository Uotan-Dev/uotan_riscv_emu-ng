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

#include "device/plic.hpp"

namespace uemu::device {

Plic::Plic(std::shared_ptr<core::Hart> hart, uint32_t ndev)
    : Device("PLIC", DEFAULT_BASE, SIZE), hart_(std::move(hart)),
      num_ids_(ndev + 1), num_ids_word_(((ndev + 1) + (32 - 1)) / 32),
      max_prio_((1u << PRIO_BITS) - 1), priority_{}, level_{} {
    contexts_.emplace_back(hart_.get(), true);
    contexts_.emplace_back(hart_.get(), false);
}

void Plic::set_interrupt_level(uint32_t id, bool lvl) {
    if (id <= 0 || num_ids_ <= id)
        return;

    uint8_t id_prio = priority_[id];
    uint32_t id_word = id / 32;
    uint32_t id_mask = 1 << (id % 32);

    std::lock_guard<std::mutex> lock(plic_mutex_);

    if (lvl)
        level_[id_word] |= id_mask;
    else
        level_[id_word] &= ~id_mask;

    for (auto& c : contexts_) {
        if (c.enable[id_word] & id_mask) {
            if (lvl) {
                c.pending[id_word] |= id_mask;
                c.pending_priority[id] = id_prio;
            } else {
                c.pending[id_word] &= ~id_mask;
                c.pending_priority[id] = 0;
                c.claimed[id_word] &= ~id_mask;
            }
            context_update(&c);
            break;
        }
    }
}

uint64_t Plic::read_internal(addr_t offset, size_t size) {
    if (size == 8)
        return read_internal(offset, 4) | (read_internal(offset + 4, 4) << 32);

    if (size != 4)
        return 0;

    std::lock_guard<std::mutex> lock(plic_mutex_);

    if (PRIORITY_BASE <= offset && offset < PENDING_BASE) {
        return priority_read(offset);
    } else if (PENDING_BASE <= offset && offset < ENABLE_BASE) {
        return pending_read(offset - PENDING_BASE);
    } else if (ENABLE_BASE <= offset && offset < CONTEXT_BASE) {
        uint32_t cntx = (offset - ENABLE_BASE) / ENABLE_PER_HART;
        offset -= cntx * ENABLE_PER_HART + ENABLE_BASE;
        if (cntx < contexts_.size())
            return context_enable_read(&contexts_[cntx], offset);
    } else if (CONTEXT_BASE <= offset && offset < SIZE) {
        uint32_t cntx = (offset - CONTEXT_BASE) / CONTEXT_PER_HART;
        offset -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
        if (cntx < contexts_.size())
            return context_read(&contexts_[cntx], offset);
    }

    return 0;
}

void Plic::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size == 8) {
        write_internal(offset, 4, value & 0xFFFFFFFF);
        write_internal(offset + 4, 4, value >> 32);
        return;
    }

    if (size != 4)
        return;

    std::lock_guard<std::mutex> lock(plic_mutex_);

    if (PRIORITY_BASE <= offset && offset < ENABLE_BASE) {
        priority_write(offset, value);
    } else if (ENABLE_BASE <= offset && offset < CONTEXT_BASE) {
        uint32_t cntx = (offset - ENABLE_BASE) / ENABLE_PER_HART;
        offset -= cntx * ENABLE_PER_HART + ENABLE_BASE;
        if (cntx < contexts_.size())
            context_enable_write(&contexts_[cntx], offset, value);
    } else if (CONTEXT_BASE <= offset && offset < SIZE) {
        uint32_t cntx = (offset - CONTEXT_BASE) / CONTEXT_PER_HART;
        offset -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
        if (cntx < contexts_.size())
            context_write(&contexts_[cntx], offset, value);
    }
}

uint32_t Plic::context_best_pending(const Context* ctx) {
    uint8_t best_id_prio = 0;
    uint32_t best_id = 0;

    for (uint32_t i = 0; i < num_ids_word_; i++) {
        if (!ctx->pending[i])
            continue;

        for (uint32_t j = 0; j < 32; j++) {
            uint32_t id = i * 32 + j;

            if (num_ids_ <= id || !(ctx->pending[i] & (1u << j)) ||
                (ctx->claimed[i] & (1u << j)))
                continue;

            if (best_id == 0 || (best_id_prio < ctx->pending_priority[id])) {
                best_id = id;
                best_id_prio = ctx->pending_priority[id];
            }
        }
    }

    if (best_id_prio <= ctx->priority_threshold)
        return 0;

    return best_id;
}

void Plic::context_update(const Context* ctx) {
    uint32_t best_id = context_best_pending(ctx);
    reg_t mask = ctx->mmode_ ? core::MIP::Field::MEIP : core::MIP::Field::SEIP;

    ctx->hart_->set_interrupt_pending(mask, best_id != 0);
}

uint32_t Plic::context_claim(Context* ctx) {
    uint32_t best_id = context_best_pending(ctx);
    uint32_t best_id_word = best_id / 32;
    uint32_t best_id_mask = (1u << (best_id % 32));

    if (best_id)
        ctx->claimed[best_id_word] |= best_id_mask;

    context_update(ctx);
    return best_id;
}

uint32_t Plic::priority_read(reg_t offset) {
    uint32_t id = offset >> 2;

    if (id > 0 && id < num_ids_)
        return priority_[id];

    return 0;
}

void Plic::priority_write(reg_t offset, uint32_t val) {
    uint32_t id = offset >> 2;

    if (id > 0 && id < num_ids_) {
        val &= (1u << PRIO_BITS) - 1;
        priority_[id] = val;
    }
}

uint32_t Plic::pending_read(reg_t offset) {
    uint32_t val = 0;
    uint32_t id_word = offset >> 2;

    if (id_word < num_ids_word_)
        for (auto& c : contexts_)
            val |= c.pending[id_word];

    return val;
}

uint32_t Plic::context_enable_read(const Context* ctx, reg_t offset) {
    uint32_t id_word = offset >> 2;

    if (id_word < num_ids_word_)
        return ctx->enable[id_word];

    return 0;
}

void Plic::context_enable_write(Context* ctx, reg_t offset, uint32_t val) {
    uint32_t id_word = offset >> 2;

    if (id_word >= num_ids_word_)
        return;

    uint32_t old_val = ctx->enable[id_word];
    uint32_t new_val = id_word == 0 ? val & ~(uint32_t)1 : val;

    ctx->enable[id_word] = new_val;

    for (uint32_t i = 0; i < 32; i++) {
        uint32_t id = id_word * 32 + i;
        uint32_t id_mask = 1u << i;
        uint8_t id_prio = priority_[id];

        if (!((old_val ^ new_val) & id_mask))
            continue;

        if ((new_val & id_mask) && (level_[id_word] & id_mask)) {
            ctx->pending[id_word] |= id_mask;
            ctx->pending_priority[id] = id_prio;
        } else if (!(new_val & id_mask)) {
            ctx->pending[id_word] &= ~id_mask;
            ctx->pending_priority[id] = 0;
            ctx->claimed[id_word] &= ~id_mask;
        }
    }

    context_update(ctx);
}

uint32_t Plic::context_read(Context* ctx, reg_t offset) {
    switch (offset) {
        case CONTEXT_THRESHOLD: return ctx->priority_threshold;
        case CONTEXT_CLAIM: return context_claim(ctx);
        default: return 0;
    };
}

void Plic::context_write(Context* ctx, reg_t offset, uint32_t val) {
    bool update = false;

    switch (offset) {
        case CONTEXT_THRESHOLD:
            val &= ((1 << PRIO_BITS) - 1);
            if (val <= max_prio_) {
                ctx->priority_threshold = val;
                update = true;
            }
            break;
        case CONTEXT_CLAIM: {
            uint32_t id_word = val / 32;
            uint32_t id_mask = 1u << (val % 32);
            if (val < num_ids_ && (ctx->enable[id_word] & id_mask)) {
                ctx->claimed[id_word] &= ~id_mask;
                update = true;
            }
            break;
        }
        default: break;
    }

    if (update)
        context_update(ctx);
}

} // namespace uemu::device
