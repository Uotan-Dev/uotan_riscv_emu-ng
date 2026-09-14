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

#include <cassert>
#include <stdexcept>

#include "core/cpu.hpp"
#include "core/decoder.hpp"

namespace uemu::core {

Cpu::Cpu(Hart& hart, MMU& mmu, std::stop_source stop_source)
    : hart_(hart), mmu_(mmu), stop_source_(std::move(stop_source)),
      mcycle_(dynamic_cast<MCYCLE*>(hart_.csrs[MCYCLE::ADDRESS].get())),
      minstret_(dynamic_cast<MINSTRET*>(hart_.csrs[MINSTRET::ADDRESS].get())) {
    assert(mcycle_ && minstret_);
}

Cpu::~Cpu() {
    stop_source_.request_stop();
    static_cast<void>(join());
}

void Cpu::start() {
    if (thread_.joinable())
        throw std::logic_error("Cpu::start() called twice");

    thread_ = std::thread(&Cpu::run, this);
}

std::exception_ptr Cpu::join() {
    if (thread_.joinable())
        thread_.join();

    return exception_;
}

void Cpu::run() {
    for (uint16_t i = 0;; i++) {
        if (stop_source_.stop_requested()) [[unlikely]]
            break;

        mcycle_->advance();

        try {
            // Normal execution
            if ((i & 0xFF) == 0 || hart_.interrupt_check_pending) [[unlikely]]
                hart_.check_interrupts();

            const auto [insn, ilen] = mmu_.ifetch();
            DecodedInsn decoded_insn = Decoder::decode(insn, ilen, hart_.pc);

            hart_.pc += static_cast<addr_t>(ilen);
            decoded_insn(hart_, mmu_);
            minstret_->advance();
        } catch (const WfiWait&) {
            // WFI: hart stalls until a locally-enabled interrupt becomes
            // pending (mip & mie != 0).
            minstret_->advance(); // WFI counts as retired

            while (true) {
                if (stop_source_.stop_requested()) [[unlikely]]
                    break;

                std::this_thread::yield();

                if (hart_.has_pending_enabled_interrupt()) {
                    try {
                        hart_.check_interrupts();
                    } catch (const Trap& trap) {
                        hart_.handle_trap(trap);
                        break;
                    }

                    break;
                }
            }
        } catch (const Trap& trap) {
            // RISC-V Traps
            hart_.handle_trap(trap);
        } catch (...) {
            exception_ = std::current_exception();
            stop_source_.request_stop();
            break;
        }
    }
}

} // namespace uemu::core
